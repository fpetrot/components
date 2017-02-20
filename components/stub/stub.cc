/*
 *  This file is part of Rabbits
 *  Copyright (C) 2017  Clement Deschamps and Luc Michel
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

#include "stub.h"

#include <rabbits/logger.h>

using namespace sc_core;

Stub::Stub(sc_core::sc_module_name name, const Parameters &params, ConfigManager &c)
    : Component(name, params, c)
    , p_target("mem")
{
    ComponentManager::Factory f;
    Parameters mem_params;

    f = c.get_component_manager().find_by_type("memory");

    if (f == nullptr) {
        MLOG(APP, ERR) << "Unable to find memory component. Please check your Rabbits installation.\n";
        return;
    }

    mem_params = f->get_params();

    Parameters p = params;
    mem_params.fill_from_description(p.get_base_description());

    if (params["trace-mem-access"].as<bool>()) {
        mem_params["disable-dmi"].set(true);
    }

    m_mem = f->create("stub-mem", mem_params);
    m_mem->get_port("mem").bind(p_target);
}

Stub::~Stub()
{
    delete m_mem;
}
