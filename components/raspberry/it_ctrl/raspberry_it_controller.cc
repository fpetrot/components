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
#include <cassert>

#include "raspberry_it_controller.h"

#include "rabbits/logger.h"

using namespace sc_core;


raspberry_it_controller::raspberry_it_controller(sc_module_name module_name,
        int it_ports, sc_signal<bool>* irq_wires, int* irq_wires_num,
        int it_ap_ports, sc_signal<bool>* irq_ap_wires, int* irq_ap_wires_num) :
        Slave(module_name)
{
    abort();
}

raspberry_it_controller::raspberry_it_controller(std::string name, ComponentParameters &params)
    : Slave(name, params)
{

    irqs_in.init(IRQ_NUM);
    declare_vector_irq_in("", irqs_in);

    m_event_list |= ev_refresh;

    /* ARM irq lines */
    declare_irq_out("irq", irq);
    declare_irq_out("fiq", fiq);

    reset_registers();
    SC_THREAD(it_thread);
}

void raspberry_it_controller::end_of_elaboration()
{
    sc_vector<sc_in<bool> >::iterator it;

    Slave::end_of_elaboration();

    /* Event list */
    for (it = irqs_in.begin(); it != irqs_in.end(); it++) {
        m_event_list |= it->value_changed_event();
    }

}

raspberry_it_controller::~raspberry_it_controller()
{
}

void raspberry_it_controller::bus_cb_write_32(uint64_t ofs, uint32_t *data, bool &bErr)
{
    uint32_t val1 = *data;
    bool refresh = false;

    DBG_PRINTF("write addr = 0x%" PRIx64 ", val = 0x%" PRIx32 " \n", ofs, val1);
    switch (ofs) {
    case IT_CONT_ENABLE_IRQ_1:
        for (int i = 0; i < 32; i++) {
            if (val1 & (1 << i)) {
                m_irq_enabled[i] = true;
                refresh = true;
            }
        }
        DBG_PRINTF("ENABLE_IRQ_1 = %" PRIx32 " \n", val1);
        break;

    case IT_CONT_ENABLE_IRQ_2:
        for (int i = 0; i < 32; i++) {
            if (val1 & (1 << i)) {
                m_irq_enabled[i+32] = true;
                refresh = true;
            }
        }
        DBG_PRINTF("ENABLE_IRQ_2 = %" PRIx32 " \n", val1);
        break;

    case IT_CONT_ENABLE_BASIC_IRQ:
        for (int i = 0; i < 32; i++) {
            if (val1 & (1 << i)) {
                m_irq_enabled[i+64] = true;
                refresh = true;
            }
            DBG_PRINTF("ENABLE_BASIC_IRQ = %" PRIx32 " \n", val1);
        }
        break;

    case IT_CONT_DISABLE_IRQ_1:
        for (int i = 0; i < 32; i++) {
            if (val1 & (1 << i)) {
                m_irq_enabled[i] = false;
                refresh = true;
            }
        }
        break;

    case IT_CONT_DISABLE_IRQ_2:
        for (int i = 0; i < 32; i++) {
            if (val1 & (1 << i)) {
                m_irq_enabled[i+32] = false;
                refresh = true;
            }
        }
        break;

    case IT_CONT_DISABLE_BASIC_IRQ:
        for (int i = 0; i < 32; i++) {
            if (val1 & (1 << i)) {
                m_irq_enabled[i+64] = false;
                refresh = true;
            }
        }
        break;

    default:
        printf("Bad %s::%s ofs=0x%" PRIx64 " , data=0x%" PRIx32 " -%" PRIx32 " !\n", name(),
                __FUNCTION__, ofs,
                *((uint32_t *) data + 0),
                *((uint32_t *) data + 1));
        exit(1);
        break;
    }
    if (refresh) {
        ev_refresh.notify();
    }
    bErr = false;
}

void raspberry_it_controller::bus_cb_read_32(uint64_t ofs, uint32_t *data, bool &bErr)
{
    uint32_t out = 0;
    uint32_t pending_in_r2, pending_in_r1;

    DBG_PRINTF("read addr = 0x%" PRIx64 " \n", ofs);

    switch (ofs) {
    case IT_CONT_ENABLE_IRQ_1:
        for (int i = 0; i < 32; i++) {
            out |= int(m_irq_enabled[i]) << i;
        }
        *data = out;
        break;

    case IT_CONT_ENABLE_IRQ_2:
        for (int i = 0; i < 32; i++) {
            out |= int(m_irq_enabled[i+32]) << i;
        }
        *data = out;
        break;

    case IT_CONT_ENABLE_BASIC_IRQ:
        for (int i = 0; i < 32; i++) {
            out |= int(m_irq_enabled[i+64]) << i;
        }
        *data = out;
        break;

    case IT_CONT_DISABLE_IRQ_1:
        for (int i = 0; i < 32; i++) {
            out |= int(!m_irq_enabled[i]) << i;
        }
        *data = out;
        break;

    case IT_CONT_DISABLE_IRQ_2:
        for (int i = 0; i < 32; i++) {
            out |= int(!m_irq_enabled[i+32]) << i;
        }
        *data = out;
        break;

    case IT_CONT_DISABLE_BASIC_IRQ:
        for (int i = 0; i < 32; i++) {
            out |= int(!m_irq_enabled[i+64]) << i;
        }
        *data = out;
        break;

    case IT_CONT_IRQ_BASIC_PENDING:
        pending_in_r1 = 0;
        pending_in_r2 = 0;

        for (int i = 0; i < 64; i++) {
            if (m_irq_pending[i]) {
                if (i < 32) {
                    pending_in_r1 = 1;
                } else {
                    pending_in_r2 = 1;
                }
            }
        }

        out = m_irq_pending[62] << 20 | m_irq_pending[57] << 19
                | m_irq_pending[56] << 18 | m_irq_pending[55] << 17
                | m_irq_pending[54] << 16 | m_irq_pending[53] << 15
                | m_irq_pending[19] << 14 | m_irq_pending[18] << 13
                | m_irq_pending[10] << 12 | m_irq_pending[9] << 11
                | m_irq_pending[7] << 10 | pending_in_r2 << 9
                | pending_in_r1 << 8;

        /* out[7:0] = m_it_pending[7:0] */
        for (int i = 0; i < 8; i++) {
            out |= m_irq_pending[i+64] << i;
        }

        DBG_PRINTF("BASIC_PENDING: %" PRIx32 " \n", out);
        *data = out;

        break;

    case IT_CONT_IRQ_PENDING_1:
        for (int i = 0; i < 32; i++) {
            out |= (int(m_irq_pending[i]) << i);
        }
        DBG_PRINTF("IRQ_PENDING_1: 0x%" PRIx32 " \n", out);
        *data = out;
        break;

    case IT_CONT_IRQ_PENDING_2:
        for (int i = 0; i < 32; i++) {
            out |= (int(m_irq_pending[i+32]) << i);
        }
        DBG_PRINTF("IRQ_PENDING_2: 0x%" PRIx32 " \n", out);
        *data = out;
        break;

    default:
        printf("Bad %s::%s ofs=0x%" PRIx64 " !\n", name(), __FUNCTION__, ofs);
        exit(1);
        break;
    }
    bErr = false;
}

void raspberry_it_controller::it_thread()
{
    bool irq_pending;
    sc_vector<sc_in<bool> >::iterator it;
    int i;

    for (;;) {
        wait(m_event_list);

        irq_pending = false;

        for (it = irqs_in.begin(), i = 0; it != irqs_in.end(); it++, i++) {
            if (it->posedge() && m_irq_enabled[i]) {
                irq_pending = m_irq_pending[i] = true;
            } else if(it->negedge()) {
                m_irq_pending[i] = false;
            }
        }

        if (irq_pending) {
            irq = true;
            DBG_PRINTF("Rising IRQ on CPU\n");
        } else {
            irq = false;
        }
    }
}

void raspberry_it_controller::reset_registers(void)
{
    for (int i = 0; i < IRQ_NUM; i++) {
        m_irq_pending[i] = m_irq_enabled[i] = false;
    }
}

