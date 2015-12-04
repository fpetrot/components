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


#include "generator.h"

#include "rabbits/logger.h"

using namespace sc_core;

Generator::Generator(sc_module_name mod_name,uint64_t start,uint64_t l,int t)
    : Master(mod_name)
{
    start_addr=start;
    length=l;
    time=t;
    SC_THREAD (thread_generator);
}


void Generator::thread_generator()
{
    uint8_t *data = new uint8_t;
    uint64_t addr;
    int i;

    wait(time,SC_NS);
    while(1) {
        addr =(uint64_t)(rand() % (length) + start_addr);
        *data = uint8_t(rand() % 1000);
        i = rand()%2;
        if (i) {
            bus_read(addr,data,4);
        } else {
            bus_write(addr,data,4);
        }
        wait(time,SC_NS);
    }
}

void Generator::bus_read(uint64_t addr, uint8_t *data, unsigned int len)
{
    Master::bus_read(addr, data, len);
}

void Generator::bus_write(uint64_t addr, uint8_t *data, unsigned int len)
{
    Master::bus_write(addr, data, len);
}

