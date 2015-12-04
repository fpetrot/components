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


#ifndef _INTERCONNECT_NOC_H
#define _INTERCONNECT_NOC_H

template <unsigned int BUSWIDTH> class InterconnectNoc;

#include <vector>

#include "rabbits/rabbits-common.h"
#include "rabbits/component/interconnect.h"
#include "rabbits/component/node.h"
#include <tlm>

#include "rabbits/logger.h"


enum {
    ABSTRACT_NOC = 0,
    MESH = 1,
    TORUS = 2,
};


template <unsigned int BUSWIDTH = 32>
class InterconnectNoc: public Interconnect<BUSWIDTH>
{
protected:
    struct RouteInfo {
        uint16_t x;
        uint16_t y;
        uint16_t z;
    };

    std::vector<Node<BUSWIDTH> *> m_nodes;
    std::vector<RouteInfo *> route_table;
    uint16_t col;
    uint16_t row;
    uint16_t Z;

    int n_type;

public:
    tlm::tlm_initiator_socket<BUSWIDTH> * get_slave_target(int i)
    {
        return m_nodes[route_table[i]->z*col*row+
                        route_table[i]->x*col+
                        route_table[i]->y]->target_slave;
    }
    uint16_t get_route_x(int i)
    {
        return route_table[i]->x;
    }
    uint16_t get_route_y(int i)
    {
        return route_table[i]->y;
    }
    uint16_t get_route_z(int i)
    {
        return route_table[i]->z;
    }
    uint16_t get_X(void)
    {
        return row;
    }
    uint16_t get_Y(void)
    {
        return col;
    }

    int decode_address_i(sc_dt::uint64 addr, sc_dt::uint64& addr_offset)
    {
        unsigned int i;
        typename Interconnect<BUSWIDTH>::AddressRange *range;

        for (i = 0; i < Interconnect<BUSWIDTH>::m_ranges.size(); i++) {
            range = Interconnect<BUSWIDTH>::m_ranges[i];
            if (addr >= range->begin && addr < range->end)
                break;
        }
        if (i == Interconnect<BUSWIDTH>::m_ranges.size()) {
            return -1;
        }

        addr_offset = range->begin;
        return i;
    }

    SC_HAS_PROCESS(InterconnectNoc);
    InterconnectNoc(sc_core::sc_module_name name) : Interconnect<BUSWIDTH>(name) {}

    virtual ~InterconnectNoc()
    {
        int i;

        for (i = 0; i < m_nodes.size(); i++) {
            delete m_nodes[i];
       }
    }

    void connect_initiator(tlm::tlm_initiator_socket<BUSWIDTH> *initiator,
                            uint16_t a, uint16_t b, uint16_t c);

    void connect_target(tlm::tlm_target_socket<BUSWIDTH> *target,
                            uint64_t addr, uint64_t len,
                            uint16_t a, uint16_t b, uint16_t c);

    //the abstract NoC
    void create_network(int type)
    {
        n_type=type;
        DBG_PRINTF("\n---------\n Abstract NoC Created \n---------\n");
    }

    void create_network(int type,uint16_t a,uint16_t b,uint16_t h);


};


#endif
