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

#include <rabbits/component/component.h>
#include <rabbits/component/port/tlm_bus.h>

#include "interconnect.h"


template <unsigned int BUSWIDTH = 32>
class BusInterconnect : public Component, public TlmBusIface<BUSWIDTH>
{
protected:
    Interconnect<BUSWIDTH> m_interco;
    std::vector<AddressRange> m_mem_map;

public:
    TlmBusPort<> bus;

    BusInterconnect(sc_core::sc_module_name name, const Parameters &params, ConfigManager &c)
        : Component(name, params, c)
        , m_interco("interco", params, c)
        , bus("mem", *this) {}

    virtual ~BusInterconnect() {}

    /* TlmBusIface */
    void connect_target(typename TlmSocketBase<BUSWIDTH>::Target &s, const AddressRange &r)
    {
         m_interco.connect_target(s, r.begin(), r.size());

         m_mem_map.push_back(r);
    }

    void connect_initiator(typename TlmSocketBase<BUSWIDTH>::Initiator &s)
    {
        m_interco.connect_initiator(s);
    }

    sc_core::sc_module* get_sc_module()
    {
        return this;
    }

    const std::vector<AddressRange> & get_memory_mapping() const
    {
        return m_mem_map;
    }
};

#endif
