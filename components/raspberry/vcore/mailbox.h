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

#ifndef _RASPBERRY_MAILBOX_H_
#define _RASPBERRY_MAILBOX_H_

#include <queue>

#include "rabbits/component/slave.h"

#define MAIL0_READ 0x0
#define MAIL0_PEAK 0x10
#define MAIL0_SENDER 0x14
#define MAIL0_STATUS 0x18
#define MAIL0_CONFIG 0x1C
#define MAIL0_WRITE  0x20

class rpi_vcore;

class vcore_mbox: public Slave
{
private:
    sc_core::sc_event irq_update;
    rpi_vcore *m_vcore;

    void bus_cb_read(uint64_t addr, uint8_t *data, unsigned int len,
            bool &bErr);
    void bus_cb_write(uint64_t addr, uint8_t *data, unsigned int len,
            bool &bErr);

    void irq_thread();

    std::queue<uint32_t> _mfifo;
    uint32_t r_config;

    void handle_fb_req(uint32_t req_addr);
public:
    SC_HAS_PROCESS (vcore_mbox);
    vcore_mbox(sc_core::sc_module_name module_name);
    virtual ~vcore_mbox();

    sc_core::sc_out<bool> irq_line;

    void set_vcore(rpi_vcore * vcore)
    {
        m_vcore = vcore;
    }
};

#endif
