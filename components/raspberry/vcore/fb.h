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

#ifndef _RASPBERRY_VCORE_FB_H_
#define _RASPBERRY_VCORE_FB_H_

#include <inttypes.h>
#include <rabbits/component/slave.h>
#include <rabbits/ui/ui_fb.h>

class rpi_vcore;

struct rpi_fb_info
{
    uint32_t physical_w, physical_h;
    uint32_t virtual_w, virtual_h;
    uint32_t bpp;
    uint32_t x_offset, y_offset;
};

class rpi_vcore_fb: public sc_core::sc_module
{
private:
    static const uint32_t FB_BASE_ADDR = 0x4c000000;

    rpi_vcore *m_vcore;

    enum fb_state_e
    {
        FB_IDLE = 0, FB_RUNNING,
    };

    fb_state_e m_fb_state;
    rpi_fb_info m_info;
    ui_fb * m_ui_fb;

public:
    SC_HAS_PROCESS(rpi_vcore_fb);
    rpi_vcore_fb(sc_core::sc_module_name mod_name);
    virtual ~rpi_vcore_fb()
    {
    }

    void set_vcore(rpi_vcore *vcore)
    {
        m_vcore = vcore;
    }
    void set_info(const rpi_fb_info & info);

    uint32_t get_pitch() const;
    uint32_t get_base_addr() const;
    uint32_t get_size() const;

    void *mem_backdoor;
};

#endif
