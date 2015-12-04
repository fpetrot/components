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

#ifndef _INTERCONNECT_H
#define _INTERCONNECT_H

#include <vector>

#include "rabbits/rabbits-common.h"

#include <tlm>

#include "rabbits/logger.h"

template <unsigned int BUSWIDTH = 32>
class Interconnect
    : public sc_core::sc_module
    , public tlm::tlm_fw_transport_if<>
    , public tlm::tlm_bw_transport_if<>
{
protected:
    struct AddressRange {
        tlm::tlm_initiator_socket<BUSWIDTH> * socket;
        uint64_t begin;
        uint64_t end;
    };

    std::vector<tlm::tlm_target_socket<BUSWIDTH> *> m_initiators;
    std::vector<tlm::tlm_initiator_socket<BUSWIDTH> *> m_targets;
    std::vector<AddressRange *> m_ranges;

    tlm::tlm_initiator_socket<BUSWIDTH> * decode_address(sc_dt::uint64 addr,
                                                         sc_dt::uint64& addr_offset)
    {
        unsigned int i;
        AddressRange *range;

        for (i = 0; i < m_ranges.size(); i++) {
            range = m_ranges[i];
            if (addr >= range->begin && addr < range->end)
                break;
        }
        if (i == m_ranges.size()) {
            return NULL;
        }

        addr_offset = range->begin;

        return range->socket;
    }

public:
    SC_HAS_PROCESS(Interconnect);
    Interconnect(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

    virtual ~Interconnect()
    {
        unsigned int i;

        for (i = 0; i < m_initiators.size(); i++) {
            delete m_initiators[i];
        }

        for (i = 0; i < m_targets.size(); i++) {
            delete m_targets[i];
        }
    }


    void connect_initiator(tlm::tlm_initiator_socket<BUSWIDTH> &initiator)
    {
        tlm::tlm_target_socket<BUSWIDTH> *socket = new tlm::tlm_target_socket<BUSWIDTH>;
        socket->bind(*this);

        m_initiators.push_back(socket);
        initiator.bind(*socket);
    }


    void connect_target(tlm::tlm_target_socket<BUSWIDTH> &target,
                        uint64_t addr, uint64_t len)
    {
        tlm::tlm_initiator_socket<BUSWIDTH> *socket = new tlm::tlm_initiator_socket<BUSWIDTH>;

        socket->bind(*this);
        socket->bind(target);

        m_targets.push_back(socket);

        AddressRange *range = new AddressRange();
        range->socket = socket;
        range->begin = addr;
        range->end = addr + len;
        m_ranges.push_back(range);
    }


    /* tlm::tlm_fw_transport_if */
    virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                    tlm::tlm_dmi& dmi_data)
    {
        bool ret;
        sc_dt::uint64 offset;
        tlm::tlm_initiator_socket<BUSWIDTH> *target;

        target = decode_address(trans.get_address(), offset);
        if (!target) {
            return false;
        }

        trans.set_address(trans.get_address() - offset);

        ret = (*target)->get_direct_mem_ptr(trans, dmi_data);

        if (ret) {
            dmi_data.set_start_address(dmi_data.get_start_address() + offset);
            dmi_data.set_end_address(dmi_data.get_end_address() + offset);
        }

        return ret;
    }

    virtual tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                               tlm::tlm_phase& phase,
                                               sc_core::sc_time& t)
    {
        ERR_PRINTF("Non-blocking transport not implemented\n");
        abort();
        return tlm::TLM_COMPLETED; 
    }

    virtual void b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        sc_dt::uint64 offset;
        tlm::tlm_initiator_socket<BUSWIDTH> *target;

        wait(3, SC_NS);

        target = decode_address(trans.get_address(), offset);
        if (!target) {
            ERR_PRINTF("Cannot find slave at address %" PRIx64 "\n",
                    static_cast<uint64_t>(trans.get_address()));
            exit(1);
        }

        trans.set_address(trans.get_address() - offset);

        (*target)->b_transport(trans, delay);

        wait(1, SC_NS);
    }

    virtual unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        sc_dt::uint64 offset;
        tlm::tlm_initiator_socket<BUSWIDTH> *target;

        target = decode_address(trans.get_address(), offset);
        if(!target) {
            return 0;
        }

        trans.set_address(trans.get_address() - offset);
        return (*target)->transport_dbg(trans);
    }


    /* tlm::tlm_bw_transport_if */
    virtual tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                               tlm::tlm_phase& phase,
                                               sc_core::sc_time& t)
    {
        ERR_PRINTF("Non-blocking transport not implemented\n");
        abort();
        return tlm::TLM_COMPLETED; 
    }

    virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range,
                                           sc_dt::uint64 end_range)
    {
        ERR_PRINTF("DMI memory invalidation not implemented\n");
        abort();
    }


};


#endif
