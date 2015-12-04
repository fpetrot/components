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
#include <inttypes.h>

#include "raspberry_arm_timer.h"

#include "rabbits/logger.h"

#define TIMER_DIV 10

using namespace sc_core;

raspberry_arm_timer::raspberry_arm_timer(sc_module_name module_name) :
        Slave(module_name)
{

    m_period = 0;
    m_mode = 0;
    m_value = 0;
    m_ns_period = 0;
    m_last_period = 0;
    m_next_period = 0;
    m_irq = false;
    m_config_mod = false;

    SC_THREAD(sl_timer_thread);
    SC_THREAD(irq_update_thread);

}

raspberry_arm_timer::raspberry_arm_timer(std::string name, ComponentParameters &params) :
        Slave(name, params)
{

    m_period = 0;
    m_mode = 0;
    m_value = 0;
    m_ns_period = 0;
    m_last_period = 0;
    m_next_period = 0;
    m_irq = false;
    m_config_mod = false;

    declare_irq_out("irq", irq);

    SC_THREAD(sl_timer_thread);
    SC_THREAD(irq_update_thread);

}

raspberry_arm_timer::~raspberry_arm_timer()
{
}

void raspberry_arm_timer::bus_cb_write(uint64_t ofs, uint8_t *data,
                    unsigned int len, bool &bErr)
{
    uint32_t value;
    uint32_t temp = 0;

    value = *((uint32_t *) data + 0);
    DBG_PRINTF("write to ofs: 0x%x val: 0x%x\n", ofs, value);
    switch (ofs) {
    case ARM_TIMER_LOAD:
        m_load = value;
        m_timerval = value;
        break;

    case ARM_TIMER_CTRL:
        DBG_PRINTF("ARM_TIME_CTRL: %x\n", value);
        m_freecounter_on = value & (1 << 9);
        m_enable_in_debug = value & (1 << 8);
        m_timer_on = value & (1 << 7);
        m_enable_it = value & (1 << 5);

        temp = (value & (0x12)) >> 2;
        switch (temp) {
        case 0:
        case 3:
            m_prescal = 1;
            break;
        case 1:
            m_prescal = 16;
            break;
        case 2:
            m_prescal = 256;
            break;
        }

        m_23bit_mode = value & (1 << 1);
        if (m_timer_on) {
            ev_wake.notify();
        }
        break;

    case ARM_TIMER_IRQ_ACK:
        m_it = 0;
        ev_irq_update.notify();
        break;

    case ARM_TIMER_RELOAD:
        m_load = value;
        break;

    default:
        DBG_PRINTF("Bad %s::%s ofs=0x%X, data=0x%X-%X!\n", name (),
                __FUNCTION__, (unsigned int) ofs,
                (unsigned int) *((uint32_t*)data + 0),
                (unsigned int) *((uint32_t*)data + 1));
        exit(1);
        break;
    }
    bErr = false;
}

void raspberry_arm_timer::bus_cb_read(uint64_t ofs, uint8_t *data, unsigned int len,
                    bool &bErr)
{
    uint32_t *val = (uint32_t *) data;
    uint32_t temp = 0;

    *val = 0;

    switch (ofs) {
    case ARM_TIMER_LOAD:
        *val = m_load;
        break;

    case ARM_TIMER_VALUE:
        *val = m_timerval;
        break;

    case ARM_TIMER_CTRL:
        /* Encoding m_prescal value */
        switch (m_prescal) {
        case 1:
            temp = 0;
            break;
        case 16:
            temp = 1;
            break;
        case 256:
            temp = 2;
            break;
        }

        *val = (m_freecounter_on << 9) | (m_enable_in_debug << 8)
                | (m_timer_on << 7) | (m_enable_it << 5) | (temp << 2)
                | (m_23bit_mode << 1);
        break;

    case ARM_TIMER_RAW_IRQ:
        *val = m_it;
        break;

    case ARM_TIMER_MASK_IRQ:
        *val = m_enable_it;
        break;

    case ARM_TIMER_RELOAD:
        *val = m_load;
        break;

    case ARM_TIMER_COUNTER:
        *val = m_freecounter;
        break;

    default:
        printf("Bad %s::%s ofs=0x%X!\n", name(), __FUNCTION__,
                (unsigned int) ofs);
        exit(1);
    }
    bErr = false;
}

void raspberry_arm_timer::irq_update_thread()
{
    while (1) {

        wait(ev_irq_update);

        irq = (m_irq == 1);
    }
}

void raspberry_arm_timer::sl_timer_thread(void)
{
    while (1) {
        if (m_timer_on) {
            uint32_t wait_time = m_ns_period * m_prescal;
            m_timerval--;
            m_freecounter++;
            if (m_timerval == 0) {
                m_timerval = m_load;
                m_irq = 1;
                m_freecounter = 0;
                ev_irq_update.notify();
            }
            DBG_PRINTF("Waiting %" PRIu32 " ns\n", wait_time);
            wait(wait_time, SC_NS);
        } else {
            wait(ev_wake);
        }
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
