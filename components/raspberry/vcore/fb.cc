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

#include "fb.h"
#include "vcore.h"

#include <rabbits/ui/ui.h>

#include "rabbits/logger.h"

using namespace sc_core;

rpi_vcore_fb::rpi_vcore_fb(sc_module_name mod_name) :
        sc_module(mod_name)
{
    m_fb_state = FB_IDLE;
    m_ui_fb = NULL;
}

void rpi_vcore_fb::set_info(const rpi_fb_info & info)
{
    ui_fb_info ui_info;

    DBG_PRINTF("setting info\n");
    DBG_PRINTF("pw: %" PRIu32 "\n", info.physical_w);
    DBG_PRINTF("ph: %" PRIu32 "\n", info.physical_h);
    DBG_PRINTF("vw: %" PRIu32 "\n", info.virtual_w);
    DBG_PRINTF("vh: %" PRIu32 "\n", info.virtual_h);
    DBG_PRINTF("bpp: %" PRIu32 "\n", info.bpp);
    DBG_PRINTF("x offset: %" PRIu32 "\n", info.x_offset);
    DBG_PRINTF("y offset: %" PRIu32 "\n", info.y_offset);

    m_info = info;

    ui_info.physical_w = info.physical_w;
    ui_info.physical_h = info.physical_h;
    ui_info.virtual_w = info.virtual_w;
    ui_info.virtual_h = info.virtual_h;
    ui_info.x_offset = info.x_offset;
    ui_info.y_offset = info.y_offset;
    ui_info.buf = ((uint8_t*) mem_backdoor)
            + m_vcore->vcore_to_arm_addr(FB_BASE_ADDR);
    ui_info.pitch = 0; /* Default value */

    switch (info.bpp) {
    case 16:
        ui_info.mode = FB_MODE_RGB565;
        break;

    case 24:
        ui_info.mode = FB_MODE_RGB888;
        break;

    default:
        ERR_PRINTF("Unable to setup framebuffer: unsupported mode\n");
        goto setup_err;
    }

    if(m_ui_fb == NULL) {
        m_ui_fb = ui::get_ui()->new_fb("raspberry framebuffer", ui_info);
    } else {
        m_ui_fb->set_info(ui_info);
    }

    m_fb_state = FB_RUNNING;
    return;

setup_err: 
    return;
}

uint32_t rpi_vcore_fb::get_pitch() const
{
    if (m_fb_state == FB_IDLE) {
        return 0;
    }

    return m_info.virtual_w * (m_info.bpp >> 3);
}

uint32_t rpi_vcore_fb::get_base_addr() const
{
    return FB_BASE_ADDR;
}

uint32_t rpi_vcore_fb::get_size() const
{
    if (m_fb_state == FB_IDLE) {
        return 0;
    }

    return get_pitch() * m_info.virtual_h;
}

