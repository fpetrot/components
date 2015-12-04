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
#include <algorithm>
#include "raspberry_system_timer.h"

#include "rabbits/logger.h"

#define TIMER_CLOCK_FV 1000000000

using namespace sc_core;

const sc_time raspberry_system_timer::PERIOD = sc_time(100, SC_NS);

raspberry_system_timer::raspberry_system_timer(sc_module_name module_name) :
        Slave(module_name)
{

    clo = chi = cmp0 = cmp1 = cmp2 = cmp3 = 0;

    SC_THREAD(timer_thread);
    SC_THREAD(irq_thread);
}

raspberry_system_timer::raspberry_system_timer(std::string name, ComponentParameters &params) :
        Slave(name, params)
{

    clo = chi = cmp0 = cmp1 = cmp2 = cmp3 = 0;

    declare_irq_out("irq", irq);

    SC_THREAD(timer_thread);
    SC_THREAD(irq_thread);
}

raspberry_system_timer::~raspberry_system_timer()
{
}

void raspberry_system_timer::bus_cb_write_32(uint64_t ofs, uint32_t *data, bool &bErr)
{
    uint32_t val1 = *data;

    DBG_PRINTF("write to ofs: 0x%x val: 0x%x\n", ofs, val1);

    if (irq) {
        DBG_PRINTF("irq\n");
    }
    switch (ofs) {
    case TIMER_CS:
        if (irq) {
            if (((val1 & 0x1)) && (cs & 0x1)) {
                cs &= 0xFE;
                DBG_PRINTF("event notify\n");
                ev_irq_update.notify();
            }
            if (((val1 & 0x2) >> 1) && ((cs & 0x2) >> 1)) {
                cs &= 0xFD;
                DBG_PRINTF("event notify\n");
                ev_irq_update.notify();
            }
            if (((val1 & 0x4) >> 2) && ((cs & 0x4) >> 2)) {
                cs &= 0xFB;
                DBG_PRINTF("event notify\n");
                ev_irq_update.notify();
            }
            if (((val1 & 0x8) >> 3) && ((cs & 0x8) >> 3)) {
                cs &= 0xF7;
                DBG_PRINTF("event notify\n");
                ev_irq_update.notify();
            }
            break;
        }
        cs = val1 & 0xF;
        break;

    case TIMER_CMP0:
        cmp0 = val1;
        ev_inval_deadline.notify();
        break;

    case TIMER_CMP1:
        cmp1 = val1;
        ev_inval_deadline.notify();
        break;

    case TIMER_CMP2:
        cmp2 = val1;
        ev_inval_deadline.notify();
        break;

    case TIMER_CMP3:
        cmp3 = val1;
        ev_inval_deadline.notify();
        break;

    default:
        ERR_PRINTF("Bad %s::%s ofs=0x%X, data=0x%X-%X!\n", name(),
                __FUNCTION__, (unsigned int) ofs,
                (unsigned int) *((uint32_t *) data + 0),
                (unsigned int) *((uint32_t *) data + 1));
        exit(1);
        break;
    }
    bErr = false;
}

void raspberry_system_timer::bus_cb_read_32(uint64_t ofs, uint32_t *data, bool &bErr)
{
    uint32_t elapsed;

    switch (ofs) {
    case TIMER_CS:
        *data = cs;
        break;

    case TIMER_CLO:
        elapsed = static_cast<uint32_t>((sc_time_stamp() - m_prev_deadline) / PERIOD);
        *data = clo + elapsed;
        break;

    case TIMER_CHI:
        *data = chi;
        break;

    case TIMER_CMP0:
        *data = cmp0;
        break;

    case TIMER_CMP1:
        *data = cmp1;
        break;

    case TIMER_CMP2:
        *data = cmp2;
        break;

    case TIMER_CMP3:
        *data = cmp3;
        break;
    default:
        ERR_PRINTF("Bad %s::%s ofs=0x%X!\n", name(), __FUNCTION__,
                (unsigned int) ofs);
        exit(1);
    }
    bErr = false;
    DBG_PRINTF("read to ofs: 0x%x, val:%" PRIx32 "\n", ofs, *data);
}

sc_time raspberry_system_timer::compute_next_deadline() const
{
    sc_time ret;
    double next_deadline = 0;

    next_deadline = static_cast<double>(std::min(std::min(cmp0-clo, cmp1-clo),
                                                 std::min(cmp2-clo, cmp3-clo)));

    DBG_PRINTF("Next deadline is %f\n", next_deadline);
    return next_deadline * PERIOD;
}

void raspberry_system_timer::timer_thread()
{
    sc_time next_deadline;
    uint32_t elapsed;

    for (;;) {
        next_deadline = compute_next_deadline();
        m_prev_deadline = sc_time_stamp();

        if (next_deadline != SC_ZERO_TIME) {
            wait(next_deadline, ev_inval_deadline);
        } else {
            wait(ev_inval_deadline);
        }

        elapsed = static_cast<uint32_t>((sc_time_stamp() - m_prev_deadline) / PERIOD);

        if (clo + elapsed < clo) {
            chi++;
        }
        clo += elapsed;

        DBG_PRINTF("clo: %" PRIx32 ", cmp3: %" PRIx32 "\n", clo, cmp3);

        if (clo != 0) {
            if (clo == cmp0) {
                cs |= 1;
                ev_irq_update.notify();
            }
            if (clo == cmp1) {
                cs |= 2;
                ev_irq_update.notify();
            }
            if (clo == cmp2) {
                cs |= 4;
                ev_irq_update.notify();
            }
            if (clo == cmp3) {
                cs |= 8;
                ev_irq_update.notify();
            }
        }
    }
}

void raspberry_system_timer::irq_thread()
{
    for(;;) {
        irq.write(cs != 0);
        if(irq) {
            DBG_PRINTF("irq\n");
        }
        wait(ev_irq_update);
    }
}
