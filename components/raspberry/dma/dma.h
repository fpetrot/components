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

#ifndef _RPI_DMA_H
#define _RPI_DMA_H

#include <queue>

#include "rabbits/component/slave.h"

class rpi_dma: public Slave
{
private:
    struct ctrl_blk
    {
        uint32_t ti;
        uint32_t src_addr;
        uint32_t dst_addr;
        uint32_t trsf_len;
        uint32_t stride;
        uint32_t nextcb;
        uint32_t reserved[2];
    };

    enum reg_offset_e
    {
        REG_CS = 0x0,
        REG_CONBLK_AD = 0x4,
        REG_TI = 0x8,
        REG_SOURCE_AD = 0xc,
        REG_DEST_AD = 0x10,
        REG_TXFR_LEN = 0x14,
        REG_STRIDE = 0x18,
        REG_NEXTCONBK = 0x1c,
        REG_DEBUG = 0x20
    };

    class dma_channel
    {
    protected:
        uint32_t m_regs[8];
    public:
        uint32_t read_reg(int reg);
        void write_reg(int reg, uint32_t v);
    };

    sc_core::sc_event irq_update;

    void bus_cb_read_32(uint64_t addr, uint32_t *data, bool &bErr);
    void bus_cb_write_32(uint64_t addr, uint32_t *data, bool &bErr);

    void irq_thread();

public:
    SC_HAS_PROCESS (rpi_dma);
    rpi_dma(sc_core::sc_module_name modname);
    virtual ~rpi_dma()
    {
    }

    //sc_core::sc_out<bool> irq_line;

};

#endif

