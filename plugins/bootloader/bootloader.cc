#include "bootloader.h"

#include <rabbits/platform/description.h>
#include <rabbits/platform/builder.h>
#include <rabbits/loader/bootloader.h>
#include <rabbits/logger.h>

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
    { 0xe320f003, ArmBootloader::FIXUP_NONE }, /* wfi */
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


void BootloaderPlugin::arm_load_blob(PlatformDescription &descr, ArmBootloader &bl)
{
    ArmBlob blob = SimpleMonoCpu;

    if (descr["blob"].is_scalar()) {
        std::string ublob = descr["blob"].as<std::string>();

        if (ublob == "simple-mono-cpu") {
            blob = SimpleMonoCpu;
        } else if (ublob == "vexpress") {
            blob = VersatileSMP;
        } else {
            LOG(APP, WRN) << "Unknown blob `" << ublob << "`. Falling back to simple-mono-cpu.\n";
        }
    }

    bl.set_entry_blob(ARM_BLOBS[blob]);

    if (blob == VersatileSMP) {
        bl.set_secondary_entry_blob(ARM_BLOBS[VersatileSMPSecondary]);
    }
}

void BootloaderPlugin::arm_bootloader(PlatformDescription &descr, PlatformBuilder &builder)
{
    ArmBootloader bl(&(builder.get_dbg_init()));
    bool has_dtb = false;
    bool has_kernel = false;

    get_app_logger().save_flags();

    if (descr["kernel-image"].is_scalar()) {
        std::string img = descr["kernel-image"].as<std::string>();
        LOG(APP, DBG) << "Loading kernel image " << img << "\n";
        bl.set_kernel_image(img);
        has_kernel = true;
    }

    if (has_kernel && (descr["kernel-load-addr"].is_scalar())) {
        uint32_t load_addr = descr["kernel-load-addr"].as<uint32_t>();
        LOG(APP, DBG) << "Setting kernel load address at 0x" << std::hex << load_addr << "\n";
        bl.set_kernel_load_addr(load_addr);
    }

    if (descr["dtb"].is_scalar()) {
        std::string img = descr["dtb"].as<std::string>();
        LOG(APP, DBG) << "Loading dtb" << img << "\n";
        bl.set_dtb(img);
        bl.set_machine_id(0xffffffff);
        has_dtb = true;
    }

    if (has_dtb && (descr["dtb-load-addr"].is_scalar())) {
        uint32_t load_addr = descr["dtb-load-addr"].as<uint32_t>();
        LOG(APP, DBG) << "Setting dtb load address at 0x" << std::hex << load_addr << "\n";
        bl.set_dtb_load_addr(load_addr);
    }

    if (descr["ram-start"].is_scalar()) {
        uint32_t ram_start = descr["ram-start"].as<uint32_t>();
        LOG(APP, DBG) << "Setting ram start address at 0x" << ram_start << "\n";
        bl.set_ram_start(ram_start);
    }

    if ((!has_dtb) && descr["machine-id"].is_scalar()) {
        uint32_t machine_id = descr["machine-id"].as<uint32_t>();
        LOG(APP, DBG) << "Setting machine id 0x" << machine_id << "\n";
        bl.set_machine_id(machine_id);
    }

    arm_load_blob(descr, bl);

    if (bl.boot()) {
        LOG(APP, ERR) << "Bootloader failed.\n";
    }

    get_app_logger().restore_flags();
}

void BootloaderPlugin::hook(const PluginHookAfterBuild& h)
{
    PlatformDescription &global_descr = *(h.get_descr());

    if (global_descr["bootloader"].type() != PlatformDescription::MAP) {
        LOG(APP, DBG) << "No bootloader configuration in description\n";
        return;
    }

    PlatformDescription &descr = global_descr["bootloader"];
    
    if (descr["architecture"].type() != PlatformDescription::SCALAR) {
        LOG(APP, ERR) << "Bootloader: missing `architecture` specifier\n";
        return;
    }

    std::string arch = descr["architecture"].as<std::string>();

    if (arch == "arm") {
        arm_bootloader(descr, *(h.get_builder()));
    } else {
        LOG(APP, ERR) << "Bootloader: Unknown architecture `" << arch << "`\n";
    }
}

