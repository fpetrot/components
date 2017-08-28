/*
 *  This file is part of Rabbits
 *  Copyright (C) 2015-2017  Clement Deschamps and Luc Michel
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

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern "C" {
#include <libfdt.h>
}

#include <rabbits/utils/loader/loader.h>
#include <rabbits/logger.h>

#include "bootloader.h"

static int get_file_size(const char *filename)
{
    int fd, size;
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return -1;
    size = lseek(fd, 0, SEEK_END);
    close(fd);
    return size;
}

static int load_file(const char *filename, uint8_t *addr)
{
    int fd, size;
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return -1;
    size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        fprintf(stderr, "file %-20s: get size error: %s\n",
                filename, strerror(errno));
        close(fd);
        return -1;
    }

    lseek(fd, 0, SEEK_SET);
    if (read(fd, addr, size) != size) {
        close(fd);
        return -1;
    }
    close(fd);
    return size;
}

static int findnode_nofail(void *fdt, const char *node_path)
{
    int offset;

    offset = fdt_path_offset(fdt, node_path);
    if (offset < 0) {
        printf("%s Couldn't find node %s: %s", __func__, node_path, fdt_strerror(offset));
        exit(1);
    }

    return offset;
}

ArmBootloader::PatchBlob::PatchBlob(const Entry blob[], size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        m_blob.push_back(blob[i]);
    }
}

void ArmBootloader::PatchBlob::patch(const uint32_t ctx[NUM_FIXUP])
{
    std::vector<Entry>::iterator it;

    for (it = m_blob.begin(); it != m_blob.end(); it++) {
        switch (it->fixup) {
        case FIXUP_NONE:
        case NUM_FIXUP:
            break;

        default:
            it->insn = ctx[it->fixup];
            break;
        }
    }
}

int ArmBootloader::PatchBlob::load(uint32_t addr, DebugInitiator *bus)
{
    std::vector<Entry>::iterator it;

    for (it = m_blob.begin(); it != m_blob.end(); it++) {
        if(bus->debug_write(addr, &(it->insn), 4) < 4) {
            LOG_F(APP, ERR, "Unable to write entry blob. Trying to write outside ram?\n");
            return 1;
        }
        addr += 4;
    }

    return 0;
}

ArmBootloader::ArmBootloader(DebugInitiator *bus)
{
    m_bus = bus;
    m_ram_start = 0;
    m_ram_size = 0;
    m_kernel_load_addr = m_initramfs_load_addr = m_dtb_load_addr = -1;
}

ArmBootloader::~ArmBootloader()
{
}

int ArmBootloader::boot()
{
    bool have_dtb = false;

    uint32_t dtb_load_addr, initramfs_load_addr, kernel_load_addr;
    uint32_t boot_data = 0, kernel_entry = 0;
    uint64_t dtb_size;

    uint32_t patch_ctx[NUM_FIXUP];

    /* Device tree */
    if (!m_dtb_path.empty()) {
        LOG_F(APP, DBG, "Loading dtb %s\n", m_dtb_path.c_str());

        if (m_dtb_load_addr != (uint32_t) -1) {
            dtb_load_addr = m_dtb_load_addr;
        } else {
            dtb_load_addr = DTB_DEFAULT_LOAD_ADDR + m_ram_start;
        }

        if(!m_bootargs.empty() || m_ram_size > 0) {
            int dt_size = get_file_size(m_dtb_path.c_str());
            if(dt_size < 0) {
                LOG_F(APP, ERR, "Unable to get size of device tree file.\n");
                return 1;
            }

            /* Expand to 2x size to give enough room for manipulation.  */
            dt_size *= 2;

            void *fdt = new uint8_t[dt_size];
            if(!fdt) {
                LOG_F(APP, ERR, "Failed to allocated DTB structure.\n");
                return 1;
            }

            int dt_file_load_size = load_file(m_dtb_path.c_str(), (uint8_t *)fdt);
            if(dt_file_load_size < 0) {
                LOG_F(APP, ERR, "Unable to open device tree file.\n");
                return 1;
            }

            int r = fdt_open_into(fdt, fdt, dt_size);
            if(r) {
                LOG_F(APP, ERR, "Unable to copy device tree in memory.\n");
                return 1;
            }

            if (fdt_check_header(fdt)) {
                LOG_F(APP, ERR, "Device tree file loaded into memory is invalid.\n");
                return 1;
            }

            if(m_ram_size > 0) {
                r = fdt_path_offset(fdt, "/memory");
                if (r < 0) {
                    int parent;
                    parent = findnode_nofail(fdt, "/");
                    r = fdt_add_subnode(fdt, parent, "memory");
                    if(r < 0) {
                        LOG_F(APP, ERR, "Couldn't create memory node in device tree.\n");
                        return 1;
                    }
                }

                if(!fdt_getprop(fdt, findnode_nofail(fdt, "/memory"), "device_type", NULL)) {
                    r = fdt_setprop_string(fdt, findnode_nofail(fdt, "/memory"), "device_type", "memory");
                    if(r < 0) {
                        LOG_F(APP, ERR, "Couldn't set device_type property.\n");
                        return 1;
                    }
                }

                int len = 0;

                uint32_t *acells_p = (uint32_t *)fdt_getprop(fdt, findnode_nofail(fdt, "/"), "#address-cells", &len);
                if(!acells_p || (len != 4) || (*acells_p == 0)) {
                    LOG_F(APP, ERR, "dtb file contains an invalid #address-cells\n");
                    return 1;
                }

                uint32_t *scells_p = (uint32_t *)fdt_getprop(fdt, findnode_nofail(fdt, "/"), "#size-cells", &len);
                if(!scells_p || (len != 4) || (*scells_p == 0)) {
                    LOG_F(APP, ERR, "dtb file contains an invalid #size-cells\n");
                    return 1;
                }

                uint32_t acells = htonl(*acells_p);
                uint32_t scells = htonl(*scells_p);
                if (acells != 1 || scells != 1) {
                    LOG_F(APP, ERR, "dtb file not compatible with rabbits (#size-cells and #address-cells must be set to 1)\n");
                    return 1;
                }

                uint32_t ram_start_be = htonl(m_ram_start);
                uint32_t ram_size_be = htonl(m_ram_size);
                uint32_t propcells[] = {ram_start_be, ram_size_be};
                uint32_t cellnum = 2;
                r = fdt_setprop(fdt, findnode_nofail(fdt, "/memory"), "reg", propcells, cellnum * sizeof(uint32_t));
                if(r < 0) {
                    LOG_F(APP, ERR, "Couldn't set reg property in /memory.\n");
                    return 1;
                }
            }

            if(!m_bootargs.empty()) {
                r = fdt_setprop_string(fdt, findnode_nofail(fdt, "/chosen"), "bootargs", m_bootargs.c_str());
                if(r < 0) {
                    LOG_F(APP, ERR, "Couldn't set bootargs in device tree.\n");
                    return 1;
                }
            }

            if (Loader::load((uint8_t *)fdt, m_bus, dtb_load_addr, dt_size)) {
                LOG_F(APP, ERR, "Unable to load dtb %s\n", m_dtb_path.c_str());
                return 1;
            }
        }
        else {
            if (Loader::load_image(m_dtb_path, m_bus, dtb_load_addr, &dtb_size)) {
                LOG_F(APP, ERR, "Unable to load dtb %s\n", m_dtb_path.c_str());
                return 1;
            }
        }
        have_dtb = true;
    }

    /* Initramfs */
    if (!m_initramfs_path.empty()) {
        LOG_F(APP, DBG, "Loading initramfs %s\n", m_initramfs_path.c_str());

        if (m_initramfs_load_addr != (uint32_t) -1) {
            initramfs_load_addr = m_initramfs_load_addr;
        } else {
            initramfs_load_addr = DTB_DEFAULT_LOAD_ADDR + m_ram_start;
            if(have_dtb) {
                initramfs_load_addr += dtb_size;
            }
        }

        if (Loader::load_image(m_initramfs_path, m_bus, initramfs_load_addr, NULL)) {
            LOG_F(APP, ERR, "Unable to load initramfs %s\n", m_initramfs_path.c_str());
            return 1;
        }
    }

    /* Kernel */
    if (!m_kernel_path.empty()) {
        LOG_F(APP, DBG, "Loading kernel %s\n", m_kernel_path.c_str());

        struct stat buf;
        if(stat(m_kernel_path.c_str(), &buf) != 0) {
            LOG_F(APP, ERR, "kernel image file not found : %s\n", m_kernel_path.c_str());
            exit(1);
        }

        /* Try to load it as an ELF first 
         * In this case, the load address is contained in the image */
        uint64_t kernel_entry64;

        LOG_F(APP, DBG, "Trying ELF...\n");
        int is_not_elf = Loader::load_elf(m_kernel_path, m_bus, &kernel_entry64);

        if(is_not_elf) {
            LOG_F(APP, DBG, "ELF failed, loading as binary image\n");

            if (m_kernel_load_addr != (uint32_t) -1) {
                kernel_load_addr = m_kernel_load_addr;
            } else {
                kernel_load_addr = KERNEL_DEFAULT_LOAD_ADDR + m_ram_start;
            }

            kernel_entry = kernel_load_addr;

            if(Loader::load_image(m_kernel_path, m_bus, kernel_load_addr, NULL)) {
                LOG_F(APP, ERR, "Unable to load kernel %s\n", m_kernel_path.c_str());
                return 1;
            }
        } else {
            kernel_entry = kernel_entry64;
        }
    }

    /* Entry blobs patching and loading */
    if (have_dtb) {
        boot_data = dtb_load_addr;
    }

    patch_ctx[FIXUP_MACHINE_ID] = m_machine_id;
    patch_ctx[FIXUP_BOOT_DATA] = boot_data;
    patch_ctx[FIXUP_KERNEL_ENTRY] = kernel_entry;
    patch_ctx[FIXUP_SMP_BOOTREG] = 0;
    patch_ctx[FIXUP_SECONDARY_ENTRY] = m_entry.size();

    if (!m_secondary_entry.empty()) {
        LOG_F(APP, DBG, "Loading secondary entry blob\n");
        m_secondary_entry.patch(patch_ctx);
        m_secondary_entry.load(m_entry.size(), m_bus);
    }

    if (!m_entry.empty()) {
        LOG_F(APP, DBG, "Loading entry blob\n");
        m_entry.patch(patch_ctx);
        m_entry.load(0, m_bus);
    }

    return 0;
}
