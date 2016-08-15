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

#ifndef _MEM_DEVICE_H_
#define _MEM_DEVICE_H_

#include <rabbits/component/slave.h>

class Memory: public Slave<>
{
protected:
    uint64_t m_size;
    bool m_readonly;
    uint8_t *m_bytes;

    void load_blob(const std::string &fn);
    void dump_to_file(const std::string &fn);

    void bus_cb_read(uint64_t addr, uint8_t *data, unsigned int len, bool &bErr);
    void bus_cb_write(uint64_t addr, uint8_t *data, unsigned int len, bool &bErr);

    virtual uint64_t debug_read(uint64_t addr, uint8_t *buf, uint64_t size);
    virtual uint64_t debug_write(uint64_t addr, const uint8_t *buf, uint64_t size);

    virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data);

public:
    const sc_core::sc_time MEM_WRITE_LATENCY;
    const sc_core::sc_time MEM_READ_LATENCY;

    Memory(sc_core::sc_module_name name, const Parameters &params, ConfigManager &c);
    virtual ~Memory();
};

#endif
