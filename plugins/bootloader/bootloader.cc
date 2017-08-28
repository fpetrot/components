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

#include <rabbits/platform/description.h>
#include <rabbits/platform/builder.h>
#include <rabbits/logger.h>

#include "bootloader.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
/* Simple bootloader that sets r0, r1, r2, and jump to kernel entry */
static const ArmBootloader::PatchBlob::Entry SIMPLE_MONO_CPU[] = {
    { 0xe3a00000, ArmBootloader::FIXUP_NONE }, /* mov     r0, #0 */
    { 0xe59f1004, ArmBootloader::FIXUP_NONE }, /* ldr     r1, [pc, #4] */
    { 0xe59f2004, ArmBootloader::FIXUP_NONE }, /* ldr     r2, [pc, #4] */
    { 0xe59ff004, ArmBootloader::FIXUP_NONE }, /* ldr     pc, [pc, #4] */
    { 0,          ArmBootloader::FIXUP_MACHINE_ID },
    { 0,          ArmBootloader::FIXUP_BOOT_DATA },
    { 0,          ArmBootloader::FIXUP_KERNEL_ENTRY }
};

/*
 * First part of the versatile express compatible bootloader.
 * It first reads the cpuid. If 0, it loads r0, r1, r2 and jump to kernel entry
 * Otherwise, it jumps to the secondary bootloader entry.
 */
static const ArmBootloader::PatchBlob::Entry VERSATILE_SMP[] = {
    { 0xee101fb0, ArmBootloader::FIXUP_NONE }, /* mrc     15, 0, r1, cr0, cr0, {5} */
    { 0xe211100f, ArmBootloader::FIXUP_NONE }, /* ands    r1, r1, #15 ; 0xf  */
    { 0x159f3004, ArmBootloader::FIXUP_NONE }, /* ldrne   r3, [pc, #4]  */
    { 0x0a000001, ArmBootloader::FIXUP_NONE }, /* beq     pc + 4 */
    { 0xe12fff13, ArmBootloader::FIXUP_NONE }, /* bx      r3  */
    { 0,          ArmBootloader::FIXUP_SECONDARY_ENTRY },

    /* First cpu */
    { 0xe3a00000, ArmBootloader::FIXUP_NONE }, /* mov     r0, #0 */
    { 0xe59f1004, ArmBootloader::FIXUP_NONE }, /* ldr     r1, [pc, #4] */
    { 0xe59f2004, ArmBootloader::FIXUP_NONE }, /* ldr     r2, [pc, #4] */
    { 0xe59ff004, ArmBootloader::FIXUP_NONE }, /* ldr     pc, [pc, #4] */
    { 0,          ArmBootloader::FIXUP_MACHINE_ID },
    { 0,          ArmBootloader::FIXUP_BOOT_DATA },
    { 0,          ArmBootloader::FIXUP_KERNEL_ENTRY }
};

/*
 * Secondary entry of the versatile express compatible bootloader.
 * It setups the interrupt controller and wait for interrupt.
 * On wakeup, it reads a fixed address to know its boot entry.
 * FIXME: gic_cpu_if and bootreg_addr should be patchable.
 */
static const ArmBootloader::PatchBlob::Entry VERSATILE_SMP_SECONDARY[] = {
    { 0xe59f2028, ArmBootloader::FIXUP_NONE }, /* ldr r2, gic_cpu_if */
    { 0xe59f0028, ArmBootloader::FIXUP_NONE }, /* ldr r0, bootreg_addr */
    { 0xe3a01001, ArmBootloader::FIXUP_NONE }, /* mov r1, #1 */
    { 0xe5821000, ArmBootloader::FIXUP_NONE }, /* str r1, [r2] - set GICC_CTLR.Enable */
    { 0xe3a010ff, ArmBootloader::FIXUP_NONE }, /* mov r1, #0xff */
    { 0xe5821004, ArmBootloader::FIXUP_NONE }, /* str r1, [r2, 4] - set GIC_PMR.Priority to 0xff */
    { 0xf57ff04f, ArmBootloader::FIXUP_NONE }, /* dsb */
    { 0xe320f002, ArmBootloader::FIXUP_NONE }, /* wfe */
    { 0xe5901000, ArmBootloader::FIXUP_NONE }, /* ldr     r1, [r0] */
    { 0xe1110001, ArmBootloader::FIXUP_NONE }, /* tst     r1, r1 */
    { 0x0afffffb, ArmBootloader::FIXUP_NONE }, /* beq     <wfi> */
    { 0xe12fff11, ArmBootloader::FIXUP_NONE }, /* bx      r1 */
    { 0x44102000, ArmBootloader::FIXUP_NONE }, /* gic_cpu_if: .word 0x.... */
    { 0x4000c204, ArmBootloader::FIXUP_NONE }  /* bootreg_addr: .word 0x.... */
};

const ArmBootloader::PatchBlob BootloaderPlugin::ARM_BLOBS[NumArmBlob] = {
    [ SimpleMonoCpu ] = ArmBootloader::PatchBlob(SIMPLE_MONO_CPU, ARRAY_SIZE(SIMPLE_MONO_CPU)),
    [ VersatileSMP ] = ArmBootloader::PatchBlob(VERSATILE_SMP, ARRAY_SIZE(VERSATILE_SMP)),
    [ VersatileSMPSecondary ] = ArmBootloader::PatchBlob(VERSATILE_SMP_SECONDARY, ARRAY_SIZE(VERSATILE_SMP_SECONDARY)),
};


void BootloaderPlugin::arm_load_blob(ArmBootloader &bl)
{
    ArmBlob blob = SimpleMonoCpu;

    std::string ublob = m_params["blob"].as<std::string>();

    if (ublob == "simple") {
        blob = SimpleMonoCpu;
    } else if (ublob == "vexpress") {
        blob = VersatileSMP;
    } else {
        MLOG(APP, WRN) << "Unknown blob `" << ublob << "`. Falling back to simple.\n";
    }

    bl.set_entry_blob(ARM_BLOBS[blob]);

    if (blob == VersatileSMP) {
        bl.set_secondary_entry_blob(ARM_BLOBS[VersatileSMPSecondary]);
    }
}

void BootloaderPlugin::arm_bootloader(PlatformBuilder &builder)
{
    ArmBootloader bl(&(builder.get_dbg_init()));
    bool has_dtb = false;
    bool has_kernel = false;

    get_app_logger().save_flags();

    if (m_params["kernel-image"].is_default()) {
        MLOG(APP, DBG) << "No kernel image provided. Skipping bootloader.\n";
        return;
    }

    std::string img = m_params["kernel-image"].as<std::string>();
    MLOG(APP, DBG) << "Loading kernel image " << img << "\n";
    bl.set_kernel_image(img);
    has_kernel = true;

    if (has_kernel && (!m_params["kernel-load-addr"].is_default())) {
        uint32_t load_addr = m_params["kernel-load-addr"].as<uint32_t>();
        MLOG(APP, DBG) << "Setting kernel load address at 0x" << std::hex << load_addr << "\n";
        bl.set_kernel_load_addr(load_addr);
    }

    if (!m_params["dtb"].is_default()) {
        std::string img = m_params["dtb"].as<std::string>();
        MLOG(APP, DBG) << "Loading dtb" << img << "\n";
        bl.set_dtb(img);
        bl.set_machine_id(0xffffffff);
        has_dtb = true;

        if(!m_params["append"].is_default()) {
            std::string bootargs = m_params["append"].as<std::string>();
            bl.set_dtb_bootargs(bootargs);
        }
    }

    if (has_dtb && (!m_params["dtb-load-addr"].is_default())) {
        uint32_t load_addr = m_params["dtb-load-addr"].as<uint32_t>();
        MLOG(APP, DBG) << "Setting dtb load address at 0x" << std::hex << load_addr << "\n";
        bl.set_dtb_load_addr(load_addr);
    }

    if (!m_params["ram-start"].is_default()) {
        uint32_t ram_start = m_params["ram-start"].as<uint32_t>();
        MLOG(APP, DBG) << "Setting ram start address at 0x" << ram_start << "\n";
        bl.set_ram_start(ram_start);
    }

    if (!m_params["ram-size"].is_default()) {
        uint32_t ram_size = m_params["ram-size"].as<uint32_t>();
        MLOG(APP, DBG) << "Setting ram size to 0x" << std::hex << ram_size << "\n";
        bl.set_ram_size(ram_size);
    }

    if ((!has_dtb) && (!m_params["machine-id"].is_default())) {
        uint32_t machine_id = m_params["machine-id"].as<uint32_t>();
        MLOG(APP, DBG) << "Setting machine id 0x" << machine_id << "\n";
        bl.set_machine_id(machine_id);
    }

    arm_load_blob(bl);

    if (bl.boot()) {
        MLOG(APP, ERR) << "Bootloader failed.\n";
    }

    get_app_logger().restore_flags();
}

void BootloaderPlugin::hook(const PluginHookAfterBuild& h)
{
    std::string arch = m_params["architecture"].as<std::string>();

    if (arch == "arm") {
        arm_bootloader(h.get_builder());
    } else {
        MLOG(APP, ERR) << "Bootloader: Unknown architecture `" << arch << "`\n";
    }
}

