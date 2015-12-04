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

#ifndef _raspberry_arm_timer_H_
#define _raspberry_arm_timer_H_

#include "rabbits/component/slave.h"

#define ARM_TIMER_LOAD          0x00
#define ARM_TIMER_VALUE         0x04
#define ARM_TIMER_CTRL          0x08
#define ARM_TIMER_IRQ_ACK       0x0C
#define ARM_TIMER_RAW_IRQ       0x10
#define ARM_TIMER_MASK_IRQ      0x14
#define ARM_TIMER_RELOAD        0x18
#define ARM_TIMER_PREDIVIDER    0x1C
#define ARM_TIMER_COUNTER       0x20

enum sl_timer_registers
{
    TIMER_VALUE = 0,
    TIMER_MODE = 1,
    TIMER_PERIOD = 2,
    TIMER_RESETIRQ = 3,
    TIMER_SPAN = 4,
};

enum sl_timer_mode
{
    TIMER_RUNNING = 1, TIMER_IRQ_ENABLED = 2,
};

class raspberry_arm_timer: public Slave
{
public:
    SC_HAS_PROCESS (raspberry_arm_timer);
    raspberry_arm_timer(sc_core::sc_module_name module_name);
    raspberry_arm_timer(std::string name, ComponentParameters &params);
    virtual ~raspberry_arm_timer();

private:
    void bus_cb_read(uint64_t addr, uint8_t *data, unsigned int len,
            bool &bErr);
    void bus_cb_write(uint64_t addr, uint8_t *data, unsigned int len,
            bool &bErr);

    void sl_timer_thread();
    void irq_update_thread();

public:
    //ports
    sc_core::sc_out<bool> irq;

private:
    uint32_t m_period;
    uint32_t m_mode;
    uint32_t m_value;
    double m_ns_period;

    sc_core::sc_event ev_wake;
    sc_core::sc_event ev_irq_update;

    uint64_t m_last_period;
    uint64_t m_next_period;

    bool m_irq;
    bool m_config_mod;

    // My vars
    uint32_t m_load;
    uint32_t m_timerval;
    uint32_t m_prescal;
    uint32_t m_freecounter;

    bool m_freecounter_on;
    bool m_enable_in_debug;
    bool m_timer_on;
    bool m_enable_it;
    bool m_23bit_mode; // if false the counter runs on 16 bits
    bool m_it;
};

#endif

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
