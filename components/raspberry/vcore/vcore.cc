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

#include "vcore.h"

#include "rabbits/logger.h"

using namespace sc_core;

rpi_vcore::rpi_vcore(sc_module_name mod_name)
	: Master(mod_name)
	, m_mbox("vcore_mailbox")
	, m_fb("vcore_framebuffer")
{
    m_mbox.set_vcore(this);
    m_fb.set_vcore(this);
}

rpi_vcore::rpi_vcore(std::string name, ComponentParameters &params)
	: Master(name, params)
	, m_mbox("vcore_mailbox")
	, m_fb("vcore_framebuffer")
{
    m_mbox.set_vcore(this);
    m_fb.set_vcore(this);

    declare_slave("mailbox", m_mbox);
    declare_irq_out("mailbox-irq", m_mbox.irq_line);
}

uint32_t rpi_vcore::vcore_to_arm_addr(uint32_t addr)
{
    /* TODO: Vcore MMU translation */
    return addr & ~0xc0000000;
}

uint32_t rpi_vcore::arm_to_vcore_addr(uint32_t addr)
{
    /* TODO: Vcore MMU translation */
    return addr | 0x40000000;
}

void rpi_vcore::bus_read(uint64_t addr, uint8_t *data, unsigned int len)
{
    Master::bus_read(vcore_to_arm_addr(addr), data, len);
}

void rpi_vcore::bus_write(uint64_t addr, uint8_t *data, unsigned int len)
{
    Master::bus_write(vcore_to_arm_addr(addr), data, len);
}

