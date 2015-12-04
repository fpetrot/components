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

#include "raspberry_gpio.h"

#include "rabbits/logger.h"

using namespace sc_core;

raspberry_gpio::raspberry_gpio(sc_module_name module_name) :
        Slave(module_name)
{
    //SC_THREAD (gpio_thread);
}

raspberry_gpio::raspberry_gpio(std::string name, ComponentParameters &params) :
        Slave(name, params)
{
}

raspberry_gpio::~raspberry_gpio()
{
}

void raspberry_gpio::bus_cb_write_32(uint64_t ofs, uint32_t *data, bool &bErr)
{
    switch (ofs) {
    case GPFSEL0:
    case GPFSEL1:
    case GPFSEL2:
    case GPFSEL3:
    case GPFSEL4:
    case GPFSEL5:
    case GPSET0:
    case GPSET1:
    case GPCLR0:
    case GPCLR1:
    case GPLEV0:
    case GPLEV1:
    case GPEDS0:
    case GPEDS1:
    case GPREN0:
    case GPREN1:
    case GPFEN0:
    case GPFEN1:
    case GPHEN0:
    case GPHEN1:
    case GPLEN0:
    case GPLEN1:
    case GPAREN0:
    case GPAREN1:
    case GPAFEN0:
    case GPAFEN1:
    case GPPUD:
    case GPPUDCLK0:
    case GPPUDCLK1:
        break;

    default:
        printf("Bad %s::%s ofs=0x%X, data=0x%X-%X!\n", name(),
                __FUNCTION__, (unsigned int) ofs,
                (unsigned int) *((uint32_t *) data + 0),
                (unsigned int) *((uint32_t *) data + 1));
        exit(1);
        break;
    }
    bErr = false;
}

void raspberry_gpio::bus_cb_read_32(uint64_t ofs, uint32_t *data , bool &bErr)
{
    switch (ofs) {
    case GPFSEL0:
    case GPFSEL1:
    case GPFSEL2:
    case GPFSEL3:
    case GPFSEL4:
    case GPFSEL5:
    case GPSET0:
    case GPSET1:
    case GPCLR0:
    case GPCLR1:
    case GPLEV0:
    case GPLEV1:
    case GPEDS0:
    case GPEDS1:
    case GPREN0:
    case GPREN1:
    case GPFEN0:
    case GPFEN1:
    case GPHEN0:
    case GPHEN1:
    case GPLEN0:
    case GPLEN1:
    case GPAREN0:
    case GPAREN1:
    case GPAFEN0:
    case GPAFEN1:
    case GPPUD:
    case GPPUDCLK0:
    case GPPUDCLK1:
        *data = 0;
        break;

    default:
        printf("Bad %s::%s ofs=0x%X!\n", name(), __FUNCTION__,
                (unsigned int) ofs);
        exit(1);
    }
    bErr = false;
}

void raspberry_gpio::gpio_thread()
{
}

