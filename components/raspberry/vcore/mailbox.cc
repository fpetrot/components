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
#include <cstring>
#include "mailbox.h"
#include "vcore.h"

#include "rabbits/logger.h"

using namespace sc_core;

vcore_mbox::vcore_mbox(sc_module_name module_name) :
        Slave(module_name)
{
    SC_THREAD(irq_thread);
    m_vcore = NULL;
}

vcore_mbox::~vcore_mbox()
{
}

#if 0
static void dump_property_buffer(uint32_t addr, uint8_t *mem)
{
    mem += addr;
    uint32_t buffer_size = *(uint32_t*) mem;
    mem += 4;
    uint32_t req_code = *(uint32_t*) mem;
    mem += 4;
    uint32_t tag;
    uint32_t v_size;
    int i = 0;

    printf("Dumping buffer at %08" PRIx32 "\n", addr);
    printf("Buffer size: %" PRIu32 " bytes\n", buffer_size);
    printf("Buffer req/rsp code: %08" PRIx32 "\n", req_code);

    tag = *(uint32_t*) mem;
    mem += 4;
    buffer_size -= 4;
    while (tag && buffer_size) {
        printf("=== Tag %d\n", i);

        printf("id: %08" PRIx32 "\n", tag);

        v_size = *(uint32_t*) mem;
        mem += 4;
        buffer_size -= 4;
        printf("size: %" PRIu32 "\n", v_size);

        v_size = *(uint32_t*) mem;
        mem += 4;
        buffer_size -= 4;
        printf("??: %" PRIu32 "\n", v_size);

        mem += v_size;
        buffer_size -= v_size;
        i++;

        tag = *(uint32_t*) mem;
        mem += 4;
        buffer_size -= 4;
    }
}
#endif

void vcore_mbox::handle_fb_req(uint32_t req_addr)
{
    rpi_fb_info info;
    const size_t BUF_SIZE = 10;
    uint32_t buf[BUF_SIZE];
    unsigned int i;

    for (i = 0; i < BUF_SIZE; i++) {
        m_vcore->bus_read(req_addr + (sizeof(uint32_t) * i),
                (uint8_t*) (buf + i), sizeof(uint32_t));
    }

    info.physical_w = buf[0];
    info.physical_h = buf[1];
    info.virtual_w = buf[2];
    info.virtual_h = buf[3];
    info.bpp = buf[5];
    info.x_offset = buf[6];
    info.y_offset = buf[7];

    m_vcore->get_fb()->set_info(info);
    buf[4] = m_vcore->get_fb()->get_pitch();
    buf[8] = m_vcore->get_fb()->get_base_addr();
    buf[9] = m_vcore->get_fb()->get_size();

    m_vcore->bus_write(req_addr + (sizeof(uint32_t) * 4),
            (uint8_t*) (buf + 4), sizeof(uint32_t));
    m_vcore->bus_write(req_addr + (sizeof(uint32_t) * 8),
            (uint8_t*) (buf + 8), sizeof(uint32_t));
    m_vcore->bus_write(req_addr + (sizeof(uint32_t) * 9),
            (uint8_t*) (buf + 9), sizeof(uint32_t));

}

void vcore_mbox::bus_cb_write(uint64_t ofs, uint8_t *data,
                                 unsigned int len, bool &bErr)
{
    uint32_t *d = (uint32_t *) data;
    uint32_t buffer_addr;
    uint32_t channel;

    DBG_PRINTF("%s to 0x%lx - value 0x%lx\n", __FUNCTION__,
            (unsigned long) ofs, (unsigned long) *d);
    switch (ofs) {
    case MAIL0_READ:
    case MAIL0_PEAK:
    case MAIL0_SENDER:
    case MAIL0_STATUS:
        break;

    case MAIL0_CONFIG:
        r_config = *d;
        break;

    case MAIL0_WRITE:
        channel = (*d & 0xf);
        buffer_addr = (*d & ~0xf);
        switch (channel) {
        case 1:
            /* Framebuffer */
            DBG_PRINTF("Framebuffer channel\n");
            handle_fb_req(buffer_addr);
            _mfifo.push(0 | channel);
            break;

        case 8:
            ERR_PRINTF("n/i msg on channel %" PRIu32 "\n", channel);
            _mfifo.push(*d);
            break;

        default:
            ERR_PRINTF("n/i msg on channel %" PRIu32 "\n", channel);
            _mfifo.push(0 | channel);
            break;
        }
        break;

    default:
        printf("Bad %s::%s ofs=0x%X, data=0x%X-%X!\n", name(),
                __FUNCTION__, (unsigned int) ofs,
                (unsigned int) *((uint32_t *) data + 0),
                (unsigned int) *((uint32_t *) data + 1));
        exit(1);
        break;
    }

    irq_update.notify();
    bErr = false;
}

void vcore_mbox::bus_cb_read(uint64_t ofs, uint8_t *data,
                             unsigned int len, bool &bErr)
{
    uint32_t *d = (uint32_t *) data;

    DBG_PRINTF("%s to 0x%lx\n", __FUNCTION__, (unsigned long) ofs);

    switch (ofs) {
    case MAIL0_READ:
        *d = _mfifo.front();
        _mfifo.pop();
        break;

    case MAIL0_PEAK:
        *d = _mfifo.front();
        break;

    case MAIL0_STATUS:
        *d = 0;
        if (!_mfifo.size()) {
            *d |= 0x40000000;
        }

        break;
    case MAIL0_SENDER:
    case MAIL0_CONFIG:
    case MAIL0_WRITE:
        *d = 0;
        break;

    default:
        printf("Bad %s::%s ofs=0x%X!\n", name(), __FUNCTION__,
                (unsigned int) ofs);
        exit(1);
    }

    irq_update.notify();
    bErr = false;
}

void vcore_mbox::irq_thread()
{
    for (;;) {
        wait(irq_update);

        irq_line = (r_config & 0x1) && _mfifo.size();
        DBG_PRINTF("New irq_line: %d\n", irq_line ? 1 : 0);
    }
}

