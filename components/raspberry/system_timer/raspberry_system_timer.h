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

#ifndef _RASPBERRY_SYSTEM_TIMER_H_
#define _RASPBERRY_SYSTEM_TIMER_H_

#include "rabbits/component/slave.h"

#define TIMER_CS        0x0
#define TIMER_CLO       0x4
#define TIMER_CHI       0x8
#define TIMER_CMP0      0xC
#define TIMER_CMP1      0x10
#define TIMER_CMP2      0x14
#define TIMER_CMP3      0x18

class raspberry_system_timer: public Slave
{
public:
    SC_HAS_PROCESS (raspberry_system_timer);
    raspberry_system_timer(sc_core::sc_module_name module_name);
    raspberry_system_timer(std::string name, ComponentParameters &params);
    virtual ~raspberry_system_timer();

private:
    void bus_cb_read_32(uint64_t addr, uint32_t *data, bool &bErr);
    void bus_cb_write_32(uint64_t addr, uint32_t *data, bool &bErr);

    void timer_thread();
    void irq_thread();
    sc_core::sc_time compute_next_deadline() const;

public:
    //ports
    sc_core::sc_out<bool> irq;

private:
    static const sc_core::sc_time PERIOD;
    uint32_t cmp0;
    uint32_t cmp1;
    uint32_t cmp2;
    uint32_t cmp3;
    uint32_t clo;
    uint32_t chi;
    uint32_t cs;

    sc_core::sc_time m_prev_deadline;

    sc_core::sc_event ev_inval_deadline;
    sc_core::sc_event ev_irq_update;

};

#endif
