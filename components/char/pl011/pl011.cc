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

#include <rabbits/logger.h>

#include "pl011.h"

using namespace sc_core;

void Pl011::read_thread()
{
    std::vector<uint8_t> data;
    int pos;

    while(1) {
        p_uart.recv(data);
        for (auto c : data) {
            MLOG_F(APP, TRC, "rcv_thread: got a char (0x%02x)\n", c);

            while (state.read_count == READ_BUF_SIZE)
                wait(evRead);

            pos = (state.read_pos + state.read_count) % READ_BUF_SIZE;
            state.read_buf[pos] = c;
            state.read_count++;
        }
        /*Interrupt RX*/
        state.int_pending = 1;
        state.int_rx = 1;
        irq_update.notify();
    }

}

void Pl011::Pl011_init_register(void)
{
    memset(&state, 0, sizeof(state));
}

Pl011::Pl011(sc_core::sc_module_name name, const Parameters &params, ConfigManager &c)
    : Slave(name, params, c)
    , p_irq("irq")
    , p_uart("uart")
{
    Pl011_init_register();

    SC_THREAD(read_thread);
    SC_THREAD(irq_update_thread);
}

Pl011::~Pl011()
{
}

void Pl011::irq_update_thread()
{
    unsigned long flags;

    while (1) {

        wait(irq_update);

        flags = (state.int_rx || state.int_tx) & state.int_pending;

        MLOG_F(SIM, DBG, "%s - %s\n", __FUNCTION__, (flags != 0) ? "1" : "0");

        p_irq.sc_p = (flags != 0);
    }
}

void Pl011::bus_cb_write(uint64_t ofs, uint8_t *data,
                                  unsigned int len, bool &bErr)
{
    uint8_t ch = 0;
    uint32_t value;
    uint32_t pos;

    bErr = false;

    ofs >>= 2;
    value = *((uint32_t *) data + 0);

#if 0
    if (ofs != 0)
#endif
    MLOG_F(SIM, DBG, "%s to 0x%lx - value 0x%lx\n", __FUNCTION__, (unsigned long) ofs,
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
                std::vector<uint8_t> data;
                data.push_back(ch);
                p_uart.send(data);
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
        MLOG_F(SIM, ERR, "%s - Error: ofs=0x%X, data=0x%X-%X!\n",
                __PRETTY_FUNCTION__, (unsigned int) ofs,
                (unsigned int) *((uint32_t *) data + 0),
                (unsigned int) *((uint32_t *) data + 1));
        bErr = true;
    }
}

void Pl011::bus_cb_read(uint64_t ofs, uint8_t *data,
                                 unsigned int len, bool &bErr)
{
    uint32_t c, *pdata;

    pdata = (uint32_t *) data;
    bErr = false;

    ofs >>= 2;
    MLOG_F(SIM, DBG, "%s to 0x%lx\n", __FUNCTION__, (unsigned long) ofs);

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
        MLOG_F(SIM, ERR, "%s - Error: ofs=0x%X!\n", __PRETTY_FUNCTION__,
                (unsigned int) ofs);

        bErr = true;
    }
}
