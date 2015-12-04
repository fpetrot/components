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

#ifndef _COMPONENTS_RABBITS_BUS_INTERCONNECT_H
#define _COMPONENTS_RABBITS_BUS_INTERCONNECT_H

#include <map>

#include "interconnect.h"
#include "bus.h"
#include "slave.h"
#include "master.h"

#include "datatypes/address_range.h"

template <unsigned int BUSWIDTH = 32>
class BusInterconnect : public Bus
{
protected:
    Interconnect<BUSWIDTH> m_interco;
    std::map<Slave*, tlm::tlm_target_socket<BUSWIDTH>* > m_target_socks;
    std::map<Master*, BusMasterIface<BUSWIDTH>* > m_init_socks;

public:
    BusInterconnect(std::string name, ComponentParameters &params)
        : Bus(name, params)
        , m_interco(sc_core::sc_module_name((name+"_interco").c_str())) {}

    virtual ~BusInterconnect() {
        typename std::map<Slave*, tlm::tlm_target_socket<BUSWIDTH>* >::iterator it_s;
        typename std::map<Master*, BusMasterIface<BUSWIDTH>* >::iterator it_m;

        for (it_s = m_target_socks.begin(); it_s != m_target_socks.end(); it_s++) {
            delete it_s->second;
        }
        for (it_m = m_init_socks.begin(); it_m != m_init_socks.end(); it_m++) {
            delete it_m->second;
        }
    }

    virtual void connect_slave(Slave &slave, AddressRange range) {
        tlm::tlm_target_socket<BUSWIDTH> *s = new tlm::tlm_target_socket<BUSWIDTH>;
        m_target_socks[&slave] = s;

        s->bind(slave);
        m_interco.connect_target(*s, range.begin(), range.size());
    }

    virtual void connect_master(Master &master) {
        BusMasterIface<BUSWIDTH> *s = new BusMasterIface<BUSWIDTH>;
        m_init_socks[&master] = s;

        s->get_socket().bind(master);
        m_interco.connect_initiator(s->get_socket());
        master.set_bus_iface(s);
    }

    virtual void connect_target_socket(tlm::tlm_target_socket<BUSWIDTH> &s, AddressRange range)
    {
        m_interco.connect_target(s, range.begin(), range.size());
    }

    virtual void connect_initiator_socket(tlm::tlm_initiator_socket<BUSWIDTH> &s)
    {
        m_interco.connect_initiator(s);
    }
};

#endif
