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
#include "raspberry_miniuart.h"

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

void raspberry_miniuart::read_thread()
{
    fd_set rfds;
    int pos, ret, nfds;
    uint8_t ch;
    struct timeval tv;

    nfds = pin + 1;

    while (1) {
        wait(10, SC_US);

        if (state.uart_enabled && state.rxint_enable) {
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

                    state.int_level |= TTY_INT_READ;
                    state.int_pending = 1;
                    irq_update.notify();
                }
            }
        }
    }
}

void raspberry_miniuart::raspberry_miniuart_init_register(void)
{
    memset(&state, 0, sizeof(state));
    state.uart_rx_enable = 1;
    state.uart_tx_enable = 1;
    state.uart_cts = 1;
    state.uart_fsm = TX_IDLE;

}

raspberry_miniuart::raspberry_miniuart(sc_module_name _name) :
        Slave(_name)
{
    int ppout[2], ppin[2];
    char spipeout[16], spipein[16];
    char logfile[strlen((const char *) _name) + 5];

    raspberry_miniuart_init_register();

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

raspberry_miniuart::~raspberry_miniuart()
{
    close(pout);
    close_ttys();
}

//void raspberry_miniuart::irq_update ()
void raspberry_miniuart::irq_update_thread()
{
    unsigned long flags;

    while (1) {

        wait(irq_update);

        flags = state.int_level & state.int_pending;

        DBG_PRINTF("%s - %s\n", __FUNCTION__, (flags != 0) ? "1" : "0");

        irq_line = (flags != 0);
    }
}

void raspberry_miniuart::cb_write(uint32_t ofs, uint8_t be, uint8_t *data,
        bool &bErr)
{
    uint8_t ch;
    uint32_t value;
    (void) ch;
    (void) value;

    bErr = false;

    ofs >>= 2;
    if (be & 0xF0) {
        ofs++;
        value = *((uint32_t *) data + 1);
    } else
        value = *((uint32_t *) data + 0);

#if 0
    if (ofs != 0)
#endif
    DBG_PRINTF("%s to 0x%lx - value 0x%lx\n", __FUNCTION__, (unsigned long) ofs,
            (unsigned long) value);

    switch (ofs) {
    case AUX_ENABLES:
        state.uart_enabled = value;
        break;

    case AUX_MU_IO_REG:
        if (state.dlab) {
            state.baudrate = (state.baudrate & (0xFF00)) | value;
        } else {
            //state.read_buf[state.read_pos] = value;
            //state.read_count++;
            //state.read_pos = (state.read_pos + 1) % READ_BUF_SIZE;
            ch = value;
            ::write(pout, &ch, 1);
        }
        break;

    case AUX_MU_IER_REG:
        if (state.dlab) {
            state.baudrate = (value << 8) | (state.baudrate & 0xFF);
        } else {
            state.txint_enable = (value >> 1) & 0x1;
            state.rxint_enable = (value & 0x1) && ((value >> 2) & 0x1);
        }
        break;

    case AUX_MU_IIR_REG:
        if ((value & 0x2) || (value & 0x4)) {
            state.read_count = 0;
            state.read_pos = 0;
        }
        break;

    case AUX_MU_LCR_REG:
        state.dlab = (value & (1 << 7)) >> 7;
        state.uart_break = (value & (1 << 6)) >> 6;
        state.data_size = (value & 1);
        break;

    case AUX_MU_MCR_REG:
        state.uart_rts = (value & 2) >> 1;
        break;

    case AUX_MU_SCRATCH:
        state.uart_scratch = value;
        break;

    case AUX_MU_CNTL_REG:
        state.uart_cts_lvl = !((value & (1 << 7)) >> 7);
        state.uart_rts_lvl = !((value & (1 << 6)) >> 6);
        state.uart_rts_autoflow_lvl = (value & (0x3 << 4)) >> 4;
        state.uart_cts_autoflow = ((value & (1 << 3)) >> 3);
        state.uart_rts_autoflow = ((value & (1 << 2)) >> 2);
        state.uart_tx_enable = ((value & (1 << 1)) >> 1);
        state.uart_rx_enable = value & 1;
        break;

    case AUX_MU_BAUD_REG:
        state.baudrate = value;
        break;

    default:
        fprintf(stderr, "%s - Error: ofs=0x%X, be=0x%X, data=0x%X-%X!\n",
                __PRETTY_FUNCTION__, (unsigned int) ofs, (unsigned int) be,
                (unsigned int) *((uint32_t *) data + 0),
                (unsigned int) *((uint32_t *) data + 1));
        exit(1);
    }
}

void raspberry_miniuart::cb_read(uint32_t ofs, uint8_t be, uint8_t *data,
        bool &bErr)
{
    uint32_t c, temp_out, *pdata;

    DBG_PRINTF("%s to 0x%lx\n", __FUNCTION__, (unsigned long) ofs);
    pdata = (uint32_t *) data;
    pdata[0] = 0;
    pdata[1] = 0;
    bErr = false;

    ofs >>= 2;
    if (be & 0xF0) {
        ofs++;
        pdata++;
    }

    switch (ofs) {
    /* AUX_IRQ */
    case AUX_IRQ:
        *pdata = state.int_pending;
        break;

        /* AUX_ENABLES */
    case AUX_ENABLES:
        *pdata = state.uart_enabled;
        break;

        /* AUX_MUX_IO_REG */
    case AUX_MU_IO_REG:
        if (state.dlab == 0) {
            c = state.read_buf[state.read_pos];
            if (state.read_count > 0) {
                state.read_count--;
                if (++state.read_pos == READ_BUF_SIZE)
                    state.read_pos = 0;

                if (0 == state.read_count) {
                    state.int_level &= ~TTY_INT_READ;
                    irq_update.notify();
                }

                evRead.notify(0, SC_NS);
            }
            *pdata = c;
        } else if (state.dlab == 1) {
            *pdata = state.baudrate & 0xFF;
            // Clear dlab ?
        }
        break;

    case AUX_MU_IER_REG:
        if (state.dlab == 0) {
            *pdata = state.rxint_enable << 2 | state.txint_enable << 1
                    | state.rxint_enable;
        } else if (state.dlab == 1) {
            *pdata = (state.baudrate & 0xFF00) >> 8;
            // Clear dlab ?
        }
        break;

    case AUX_MU_IIR_REG:
        *pdata = (3 << 6)
                || (state.fifo_rxmode << 2) | (state.fifo_txmode << 1)
                        | (!state.int_pending);
        break;

    case AUX_MU_LCR_REG:
        *pdata = (state.dlab << 7) | (state.uart_break << 6) | state.data_size;
        break;

    case AUX_MU_MCR_REG:
        *pdata = state.uart_rts << 1;
        break;

    case AUX_MU_LSR_REG:
        *pdata = ((state.uart_fsm == TX_IDLE) << 6)
                | ((state.uart_fsm == TX_EMPTY) << 5)
                | ((state.uart_fsm == RX_OVERRUN) << 1)
                | ((state.uart_fsm == DATA_RDY));
        break;

    case AUX_MU_MSR_REG:
        *pdata = (!state.uart_cts) << 5;
        break;

    case AUX_MU_SCRATCH:
        *pdata = state.uart_scratch;
        DBG_PRINTF("--->0x%lx\n", (unsigned long) *pdata);
        break;

    case AUX_MU_CNTL_REG:
        temp_out = (!state.uart_cts_lvl) << 7 | (!state.uart_rts_lvl) << 6
                | state.uart_rts_autoflow_lvl << 4
                | state.uart_cts_autoflow << 3 | state.uart_rts_autoflow << 2
                | state.uart_tx_enable << 1 | state.uart_rx_enable;
        *pdata = temp_out;
        DBG_PRINTF("--->0x%lx\n", (unsigned long) *pdata);
        break;

    case AUX_MU_STAT_REG:
        *pdata = ((state.fifo_txmode ? (state.read_count & 0xF) : 0) << 24)
                | ((state.fifo_rxmode ? (state.read_count & 0xF) : 0) << 16)
                | ((state.read_count == 0) && (state.uart_fsm == TX_IDLE)) << 9
                | ((state.read_count == 0) << 8) | ((state.uart_cts) << 7)
                | (state.uart_rts << 6)
                | (state.read_count == READ_BUF_SIZE) << 5
                | ((state.uart_fsm == RX_OVERRUN) << 4)
                | ((state.uart_fsm == TX_IDLE) << 3)
                | ((state.uart_fsm == RX_IDLE) << 2)
                | ((state.read_count < READ_BUF_SIZE) << 1)
                | (state.read_count != 0);
        break;

    case AUX_MU_BAUD_REG:
        *pdata = state.baudrate;
        DBG_PRINTF("--->0x%lx\n", (unsigned long) *pdata);
        break;

    default:
        fprintf(stderr, "%s - Error: ofs=0x%X, be=0x%X!\n", __PRETTY_FUNCTION__,
                (unsigned int) ofs, (unsigned int) be);
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
