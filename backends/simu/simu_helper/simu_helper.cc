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

#include "simu_helper.h"

#include <cstdio>
#include <cstdlib>

#include <fstream>

#include <rabbits/logger.h>

using namespace sc_core;

SimuHelper::SimuHelper(sc_core::sc_module_name name, const Parameters &params, ConfigManager &c)
    : Slave(name, params, c)
{
}

SimuHelper::~SimuHelper()
{
}

void SimuHelper::bus_cb_read(uint64_t addr, uint8_t *data,
                             unsigned int len, bool &bErr)
{
    *data = 0;
}

void SimuHelper::bus_cb_write(uint64_t addr, uint8_t *data,
                              unsigned int len, bool &bErr)
{
    sc_stop();
}
