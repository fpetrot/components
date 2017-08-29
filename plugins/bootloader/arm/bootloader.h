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

#ifndef _UTILS_BOOTLOADER_H
#define _UTILS_BOOTLOADER_H

#include "rabbits/component/debug_initiator.h"

#include <vector>

/**
 * @file bootloader.h
 * ArmBootloader class declaration.
 */

/**
 * @brief ARM bootloader simulation.
 *
 * This class is used to simulate the behavior of a bootloader running on an
 * ARM system, such as U-Boot.
 *
 * It can load into the platform memory some binaries or ELF files, like a
 * Linux kernel, a device tree, etc.
 *
 * It can also patch and load a binary blob with configuration values such as
 * machine id, kernel entry address...
 *
 * It needs a DebugInitiator to write into the platform memory.
 */
class ArmBootloader
{
public:
    /**
     * @brief Default device tree loading address, relative to the start address of the memory.
     */
    static const uint32_t DTB_DEFAULT_LOAD_ADDR = 128 * 1024 * 1024;

    /**
     * @brief Default Linux kernel loading address, relative to the start address of the memory.
     */
    static const uint32_t KERNEL_DEFAULT_LOAD_ADDR = 32 * 1024 * 1024;

    /**
     * @brief Fixups used when patching a blob entry
     */
    enum fixup_e {
        FIXUP_NONE = 0,         /**< No fixup for this entry. */
        FIXUP_MACHINE_ID,       /**< Patch with the machine ID. */
        FIXUP_BOOT_DATA,        /**< Patch with the boot data loading address (dtb or atag structure). */
        FIXUP_KERNEL_ENTRY,     /**< Patch with the kernel entry address. */
        FIXUP_SMP_BOOTREG,      /**< Patch with the SMP boot register address. */
        FIXUP_SECONDARY_ENTRY,  /**< Patch with the SMP secondary blob load address */

        NUM_FIXUP
    };


    /**
     * @brief A binary blob to be patched.
     */
    class PatchBlob
    {
    public:
        /**
         * @brief A 32-bit entry in a PatchBlob
         */
        struct Entry
        {
            uint32_t insn; /**< The 32-bit value.  */
            fixup_e  fixup; /**< The desired fixup. */
        };

    protected:
        std::vector<Entry> m_blob;

    public:
        PatchBlob() {}
        PatchBlob(const Entry blob[], size_t size);

        void patch(const uint32_t patch_ctx[NUM_FIXUP]);

        bool empty() { return m_blob.empty(); }
        uint32_t size() { return m_blob.size() * 4; }
        int load(uint32_t addr, DebugInitiator *bus);
    };

protected:
    ConfigManager &m_config;
    PatchBlob m_entry;
    PatchBlob m_secondary_entry;
    DebugInitiator *m_bus;

    uint32_t m_machine_id;
    uint32_t m_ram_start;
    uint32_t m_ram_size;

    std::string m_kernel_path, m_initramfs_path, m_dtb_path, m_bootargs;
    uint32_t m_kernel_load_addr, m_initramfs_load_addr, m_dtb_load_addr;

    uint64_t load_dtb(uint32_t &load_addr);
public:
    ArmBootloader(ConfigManager &config, DebugInitiator *bus);
    virtual ~ArmBootloader();

    /**
     * @brief Load a binary image into memory at the given address.
     *
     * @param[in] path Path of the binary image file.
     * @param[in] load_addr Load address.
     *
     * @return The number of bytes effectively written into the platform memory.
     */
    int load_image(const std::string & path, uint64_t load_addr);

    /**
     * @brief Set the kernel image to load.
     *
     * @param[in] path Path to the kernel image.
     */
    void set_kernel_image(const std::string & path) { m_kernel_path = path; }

    /**
     * @brief Set the kernel load address.
     *
     * @param[in] addr Kernel load address.
     */
    void set_kernel_load_addr(uint32_t addr) { m_kernel_load_addr = addr; }

    /**
     * @brief Set the initramfs image to load.
     *
     * @param[in] path Path to the initramfs image.
     */
    void set_initramfs_image(const std::string & path) { m_initramfs_path = path; }

    /**
     * @brief Set the initramfs load address.
     *
     * @param[in] addr Initramfs load address.
     */
    void set_initramfs_load_addr(uint32_t addr) { m_initramfs_load_addr = addr; }

    /**
     * @brief Set the device tree image to load.
     *
     * @param[in] path Path to the device image.
     */
    void set_dtb(const std::string & path) { m_dtb_path = path; }

    /**
     * @brief Set the kernel cmdline
     *
     * @param[in] bootargs Kernel cmdline to write in the device tree
     */
    void set_dtb_bootargs(const std::string & bootargs) { m_bootargs = bootargs;}

    /**
     * @brief Set the device tree load address.
     *
     * @param[in] addr Device tree load address.
     */
    void set_dtb_load_addr(uint32_t addr) { m_dtb_load_addr = addr; }


    /**
     * @brief Set the ARM machine ID.
     *
     * @param[in] machine_id The machine ID.
     */
    void set_machine_id(uint32_t machine_id) { m_machine_id = machine_id; }

    /**
     * @brief Set the entry blob.
     *
     * This blob is loaded at address 0 and is executed when the processor cold
     * boots or resets. It is patched before being written into memory.
     *
     * @param[in] blob The entry blob to patch and load.
     */
    void set_entry_blob(const PatchBlob &blob) { m_entry = PatchBlob(blob); }

    /**
     * @brief Set the secondary entry blob.
     *
     * This blob is loaded after the entry blob and is used on SMP systems for
     * secondary processors.
     *
     * @param[in] blob The secondary entry blob to patch and load.
     */
    void set_secondary_entry_blob(const PatchBlob &blob) { m_secondary_entry = PatchBlob(blob); }

    /**
     * @brief Set the RAM start address of the platform.
     *
     * @param[in] ram_start The start address of the RAM.
     */
    void set_ram_start(uint32_t ram_start) { m_ram_start = ram_start; }

    /**
     * @brief Set the RAM size of the platform.
     *
     * @param[in] ram_size The size of the RAM.
     */
    void set_ram_size(uint32_t ram_size) { m_ram_size = ram_size; }

    /**
     * @brief Perform the bootloading steps.
     *
     * @return 0 on success, a positive value on error.
     */
    int boot();
};

#endif
