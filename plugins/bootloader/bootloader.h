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

#ifndef _PLUGINS_BOOTLOADER_H
#define _PLUGINS_BOOTLOADER_H

#include <rabbits/plugin/plugin.h>
#include <rabbits/loader/bootloader.h>

class BootloaderPlugin : public Plugin {
protected:
    enum ArmBlob {
        SimpleMonoCpu,
        VersatileSMP,
        VersatileSMPSecondary,
        NumArmBlob
    };

    static const ArmBootloader::PatchBlob ARM_BLOBS[NumArmBlob];

    void arm_load_blob(ArmBootloader &bl);
    void arm_bootloader(PlatformBuilder &builder);

public:
    BootloaderPlugin(const std::string & name, const Parameters & params, ConfigManager &c)
        : Plugin(name, params, c)
    {
        c.add_param_alias("kernel", m_params["kernel-image"]);
        c.add_param_alias("dtb", m_params["dtb"]);
    }

    virtual ~BootloaderPlugin() {}

    virtual void hook(const PluginHookAfterBuild& h);
};

#endif
