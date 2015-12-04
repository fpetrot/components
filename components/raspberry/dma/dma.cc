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

#include "dma.h"

#include "rabbits/rabbits-common.h"
#include "rabbits/logger.h"

using namespace sc_core;

/* === rpi_dma::dma_channel === */

uint32_t rpi_dma::dma_channel::read_reg(int reg)
{
    return 0;
}

void rpi_dma::dma_channel::write_reg(int reg, uint32_t v)
{

}

/* === rpi_dma === */

rpi_dma::rpi_dma(sc_module_name modname) :
        Slave(modname)
{

}

void rpi_dma::bus_cb_write_32(uint64_t addr, uint32_t *data, bool &bErr)
{
}

void rpi_dma::bus_cb_read_32(uint64_t addr, uint32_t *data, bool &bErr)
{
    *data = 0;
}

