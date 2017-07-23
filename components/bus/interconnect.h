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

#include <systemc>
#include <tlm>

#include <rabbits/component/component.h>
#include <rabbits/config/manager.h>
#include <rabbits/logger.h>

template <unsigned int BUSWIDTH = 32>
class Interconnect
    : public Component
    , public tlm::tlm_fw_transport_if<>
    , public tlm::tlm_bw_transport_if<>
{
public:
    typedef tlm::tlm_base_target_socket_b<BUSWIDTH,
                                          tlm::tlm_fw_transport_if<>,
                                          tlm::tlm_bw_transport_if<> > BaseTargetSocket;

    typedef tlm::tlm_base_initiator_socket_b<BUSWIDTH,
                                             tlm::tlm_fw_transport_if<>,
                                             tlm::tlm_bw_transport_if<> > BaseInitiatorSocket;

protected:
    struct TargetMapping {
        int target_index;
        uint64_t begin;
        uint64_t end;
    };

    std::vector<TargetMapping> m_ranges;

    tlm::tlm_target_socket<BUSWIDTH, tlm::tlm_base_protocol_types, 0> m_target;
    tlm::tlm_initiator_socket<BUSWIDTH, tlm::tlm_base_protocol_types, 0> m_initiator;

    int decode_address(sc_dt::uint64 addr,
                       sc_dt::uint64& addr_offset)
    {
        for (auto &range: m_ranges) {
            if (addr >= range.begin && addr < range.end) {
                addr_offset = range.begin;
                return range.target_index;
            }
        }

        return -1;
    }

public:
    SC_HAS_PROCESS(Interconnect);
    Interconnect(sc_core::sc_module_name name, const Parameters &p, ConfigManager &c)
        : Component(name, p, c)
        , m_target("bus_target_socket")
        , m_initiator("bus_initiator_socket")
    {
        m_target.bind(*this);
        m_initiator.bind(*this);
    }

    virtual ~Interconnect()
    {
    }


    void connect_initiator(BaseInitiatorSocket &initiator)
    {
        m_target.bind(initiator);
    }


    void connect_target(BaseTargetSocket &target,
                        uint64_t addr, uint64_t len)
    {
        TargetMapping range;

        /* XXX This piece of code relies on non-standard SystemC behavior.
         * It works with the Accellera reference implementation but is not
         * guaranteed to work with others. */
        range.target_index = m_initiator.size();

        range.begin = addr;
        range.end = addr + len;
        m_ranges.push_back(range);

        m_initiator.bind(target);
    }


    /* tlm::tlm_fw_transport_if */
    virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                    tlm::tlm_dmi& dmi_data)
    {
        bool ret;
        sc_dt::uint64 offset;

        int target_index = decode_address(trans.get_address(), offset);

        if (target_index == -1) {
            return false;
        }

        trans.set_address(trans.get_address() - offset);

        ret = m_initiator[target_index]->get_direct_mem_ptr(trans, dmi_data);

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
        MLOG_F(SIM, ERR, "Non-blocking transport not implemented\n");
        abort();
        return tlm::TLM_COMPLETED;
    }

    virtual void b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        sc_dt::uint64 offset;

        wait(3, sc_core::SC_NS);

        int target_index = decode_address(trans.get_address(), offset);
        if (target_index == -1) {
            Parameters & globals = m_config.get_global_params();
            if (globals["report-non-mapped-access"].as<bool>()) {
                MLOG_F(SIM, ERR, "Cannot find target at address 0x%" PRIx64 "\n",
                       static_cast<uint64_t>(trans.get_address()));
            }
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

	MLOG_F(SIM, TRC, "Memory request at address 0x%08" PRIx64 "\n", trans.get_address());

        trans.set_address(trans.get_address() - offset);

        m_initiator[target_index]->b_transport(trans, delay);

        wait(1, sc_core::SC_NS);
    }

    virtual unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        sc_dt::uint64 offset;

        int target_index = decode_address(trans.get_address(), offset);
        if(target_index == -1) {
            return 0;
        }

        trans.set_address(trans.get_address() - offset);
        return m_initiator[target_index]->transport_dbg(trans);
    }


    /* tlm::tlm_bw_transport_if */
    virtual tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                               tlm::tlm_phase& phase,
                                               sc_core::sc_time& t)
    {
        MLOG_F(SIM, ERR, "Non-blocking transport not implemented\n");
        abort();
        return tlm::TLM_COMPLETED;
    }

    virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range,
                                           sc_dt::uint64 end_range)
    {
        MLOG_F(SIM, ERR, "DMI memory invalidation not implemented\n");
        abort();
    }


};

#endif
