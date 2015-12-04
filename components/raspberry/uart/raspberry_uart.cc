/*
 *  This file is part of Rabbits
 *  Copyright (C) 2015  Clement Deschamps and Luc Michel
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "raspberry_uart.h"
#include "rabbits/logger.h"

#if 0
#define DEBUG_DEVICE_TTY_SERIAL
#endif

#include "rabbits/logger.h"

#define TTY_INT_READ        1

using namespace sc_core;

static int s_pid_tty[256];
static int s_nb_tty = 0;

static void close_ttys()
{
    int i, status;

    if (s_nb_tty == 0)
        return;

    for (i = 0; i < s_nb_tty; i++) {
        kill(s_pid_tty[i], SIGKILL);
        ::wait(&status);
    }

    s_nb_tty = 0;
}

static void sig_hup(int)
{
    exit(1);
}

void raspberry_uart::read_thread()
{
    fd_set rfds;
    int pos, ret, nfds;
    uint8_t ch;
    struct timeval tv;

    nfds = pin + 1;

    while (1) {
        wait(10, SC_US);

        if (state.uart_enabled && state.uart_rx_enable
                && UART_MASK_RX(state.irq_mask)) {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO(&rfds);
            FD_SET(pin, &rfds);
            ret = select(nfds, &rfds, NULL, NULL, &tv);
            if (ret > 0) {
                if (FD_ISSET(pin, &rfds)) {
                    ::read(pin, &ch, 1);
                    DBG_PRINTF("%s read 0x%x, %c\n", __FUNCTION__,
                            (unsigned int) ch, ch);

                    while (state.read_count == READ_BUF_SIZE)
                        wait(evRead);

                    pos = (state.read_pos + state.read_count) % READ_BUF_SIZE;
                    state.read_buf[pos] = ch;
                    state.read_count++;

                    /*Interrupt RX*/
                    state.int_pending = 1;
                    state.int_rx = 1;
                    irq_update.notify();
                }
            }
        }
    }
}

void raspberry_uart::raspberry_uart_init_register(void)
{
    memset(&state, 0, sizeof(state));
}

raspberry_uart::raspberry_uart(sc_module_name _name) :
        Slave(_name)
{
    int ppout[2], ppin[2];
    char spipeout[16], spipein[16];
    char logfile[strlen((const char *) _name) + 5];

    raspberry_uart_init_register();

    signal(SIGHUP, sig_hup);
    atexit(close_ttys);

    signal(SIGPIPE, SIG_IGN);

    if (pipe(ppout) < 0) {
        ERR_STREAM(name() << " can't open out pipe!" << std::endl);
        exit(0);
    }
    if (pipe(ppin) < 0) {
        ERR_STREAM(name() << " can't open in pipe!" << std::endl);
        exit(0);
    }

    pout = ppout[1];
    sprintf(spipeout, "%d", ppout[0]);
    pin = ppin[0];
    sprintf(spipein, "%d", ppin[1]);

    sprintf(logfile, "%s-log", (const char *) _name);

    if (!(s_pid_tty[s_nb_tty++] = fork())) {
        setpgrp();

        if (execlp("xterm", "xterm", "-sb", "-sl", "1000", "-l", "-lf", logfile,
                "-n", (const char *) _name, "-T", (const char *) _name, "-e",
                "tty_term_rw", spipeout, spipein,
                NULL) == -1) {
            perror("tty_term: execlp failed!");
            _exit(1);
        }
    }

    SC_THREAD(read_thread);
    SC_THREAD(irq_update_thread);
}

raspberry_uart::raspberry_uart(std::string name, ComponentParameters &params) :
        Slave(name, params)
{
    raspberry_uart_init_register();

    declare_irq_out("irq", irq_line);
    SC_THREAD(read_thread);
    SC_THREAD(irq_update_thread);
}

raspberry_uart::~raspberry_uart()
{
    close(pout);
    close_ttys();
}

void raspberry_uart::irq_update_thread()
{
    unsigned long flags;

    while (1) {

        wait(irq_update);

        flags = (state.int_rx || state.int_tx) & state.int_pending;

        DBG_PRINTF("%s - %s\n", __FUNCTION__, (flags != 0) ? "1" : "0");

        irq_line = (flags != 0);
    }
}

void raspberry_uart::bus_cb_write(uint64_t ofs, uint8_t *data,
			    unsigned int len, bool &bErr)
{
    uint8_t ch;
    uint32_t value;
    uint32_t pos;

    bErr = false;

    ofs >>= 2;
    value = *((uint32_t *) data + 0);

#if 0
    if (ofs != 0)
#endif
    DBG_PRINTF("%s to 0x%lx - value 0x%lx\n", __FUNCTION__, (unsigned long) ofs,
            (unsigned long) value);

    switch (ofs) {

    case UART_DR:
        if (true || state.uart_enabled) {
            if (state.uart_loopback) {
                /*fifos disable*/
                if (!(state.lcrh >> 4) & 1) {
                    state.read_single = value;
                    /*clear tx interrupt*/
                    if (state.int_pending & state.int_tx) {
                        state.int_tx = 0;
                        if (state.int_rx == 0) {
                            state.int_pending = 0;
                            irq_update.notify();
                        }
                    }
                } else {
                    while (state.read_count == READ_BUF_SIZE)
                        wait(evRead);

                    pos = (state.read_pos + state.read_count) % READ_BUF_SIZE;
                    state.read_buf[pos] = ch;
                    state.read_count++;
                }
            } else {
                ch = value;
                ::write(1, &ch, 1);
            }
        }
        break;

    case UART_RSRECR:
    case UART_FR:
    case UART_ILPR:
        break;

    case UART_IBRD:
        state.uart_baudrate_divisor = value & 0xFFFF;
        break;

    case UART_FBRD:
        state.uart_frac_baudrate_divisor = value & 0x3F;
        break;

    case UART_LCRH:
        state.data_size = ((value >> 5) & 0x3) + 5;
        state.lcrh = value & 0xFF;
        /*set interrupt tx*/
        /*fifos enable : perfect fifos -> tx interrupt*/
        /*fifos disable -> tx interrupt*/
        if (UART_MASK_TX(state.irq_mask)) {
            state.int_tx = 1;
            state.int_pending = 1;
            irq_update.notify();
        }
        break;

    case UART_CR:
        state.cts_enable = (value >> 15) & 1;
        state.rts_enable = (value >> 14) & 1;
        state.uart_rx_enable = (value >> 9) & 1;
        state.uart_tx_enable = (value >> 8) & 1;
        state.uart_loopback = (value >> 7) & 1;
        state.uart_enabled = value & 1;
        break;

    case UART_IFLS:
        state.rx_irq_lvl = (value >> 3) & 0x7;
        state.tx_irq_lvl = value & 0x7;
        break;

    case UART_IMSC:
        state.irq_mask = value & 0x7FF;
        if (UART_MASK_TX(state.irq_mask) == 1) {
            state.int_tx = 1;
            state.int_pending = 1;
            irq_update.notify();
        } else {
            state.int_tx = 0;
            if (state.int_rx == 0) {
                state.int_pending = 0;
                irq_update.notify();
            }
        }
        break;

    case UART_ICR:
        if (state.int_rx && ((UART_MASK_RX(value)))) {
            state.int_rx = 0;
            if (state.int_tx == 0) {
                state.int_pending = 0;
                irq_update.notify();
            }
        }
        if (state.int_tx && ((UART_MASK_TX(value)))) {
            state.int_tx = 0;
            if (state.int_rx == 0) {
                state.int_pending = 0;
                irq_update.notify();
            }
        }

        break;

    case UART_DMACR:
    case UART_ITCR:
    case UART_ITIP:
    case UART_ITOP:
    case UART_TDR:
    case UART_MIS:
        break;

    default:
        fprintf(stderr, "%s - Error: ofs=0x%X, data=0x%X-%X!\n",
                __PRETTY_FUNCTION__, (unsigned int) ofs,
                (unsigned int) *((uint32_t *) data + 0),
                (unsigned int) *((uint32_t *) data + 1));
        exit(1);
    }
}

void raspberry_uart::bus_cb_read(uint64_t ofs, uint8_t *data,
			    unsigned int len, bool &bErr)
{
    uint32_t c, *pdata;

    pdata = (uint32_t *) data;
    bErr = false;

    ofs >>= 2;
    DBG_PRINTF("%s to 0x%lx\n", __FUNCTION__, (unsigned long) ofs);

    switch (ofs) {
    case UART_DR:
        /*fifos enable*/
        if ((state.lcrh >> 4) & 1) {
            c = state.read_buf[state.read_pos];
            if (state.read_count > 0) {
                state.read_count--;
                if (++state.read_pos == READ_BUF_SIZE) {
                    state.read_pos = 0;
                }

                if (0 == state.read_count) {
                    state.int_rx = 0;
                    if (state.int_tx == 0) {
                        state.int_pending = 0;
                        irq_update.notify();
                    }
                }

                evRead.notify(0, SC_NS);
            }
        } else {
            c = state.read_single;
        }
        *pdata = c;
        break;

    case UART_RSRECR:
        *pdata = 0;
        break;

    case UART_FR:
        int buf_full, buf_empty;
        buf_full = (state.read_count == READ_BUF_SIZE) ? 1 : 0;
        buf_empty = (state.read_count == 0) ? 1 : 0;
        *pdata = 1 << 7 | buf_full << 6 | buf_empty << 4;
        break;

    case UART_ILPR:
        *pdata = 0;
        break;

    case UART_IBRD:
        *pdata = state.uart_baudrate_divisor;
        break;

    case UART_FBRD:
        *pdata = state.uart_frac_baudrate_divisor;
        break;

    case UART_LCRH:
        *pdata = state.lcrh;
        break;

    case UART_CR:
        *pdata = state.uart_rx_enable << 9 | state.uart_tx_enable << 8
                | state.uart_loopback << 7 | state.uart_enabled;
        break;

    case UART_IFLS:
        *pdata = state.rx_irq_lvl << 3 | state.tx_irq_lvl;
        break;

    case UART_IMSC:
        *pdata = state.irq_mask;
        break;

    case UART_RIS:
        *pdata = state.int_rx << 4 | state.int_tx << 5;
        break;

    case UART_MIS:
        /*type of interrupt*/
        *pdata = ((state.int_rx && UART_MASK_RX(state.irq_mask)) << 4)
                | ((state.int_tx && UART_MASK_TX(state.irq_mask)) << 5);
        break;

    case UART_ICR:
        *pdata = 0;
        break;

    case UART_DMACR:
        *pdata = 0;
        break;

    case UART_ITCR:
        *pdata = 0;
        break;

    case UART_ITIP:
        *pdata = 0;
        break;

    case UART_ITOP:
        *pdata = 0;
        break;

    case UART_TDR:
        *pdata = 0;
        break;

    case UART_PID0:
        *pdata = PID & 0xFF;
        break;

    case UART_PID1:
        *pdata = (PID >> 8) & 0xFF;
        break;

    case UART_PID2:
        *pdata = (PID >> 16) & 0xFF;
        break;

    case UART_PID3:
        *pdata = (PID >> 24) & 0xFF;
        break;

    case UART_CID0:
        *pdata = AMBA_CID & 0xFF;
        break;

    case UART_CID1:
        *pdata = (AMBA_CID >> 8) & 0xFF;
        break;

    case UART_CID2:
        *pdata = (AMBA_CID >> 16) & 0xFF;
        break;

    case UART_CID3:
        *pdata = (AMBA_CID >> 24) & 0xFF;
        break;

    default:
        fprintf(stderr, "%s - Error: ofs=0x%X!\n", __PRETTY_FUNCTION__,
                (unsigned int) ofs);

        exit(1);
    }
}

/*
 * Vim standard variables
 * vim:set ts=4 expandtab tw=80 cindent syntax=c:
 *
 * Emacs standard variables
 * Local Variables:
 * mode: c
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
