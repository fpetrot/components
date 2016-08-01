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

#include <rabbits/test/test.h>
#include <rabbits/test/slave_tester.h>

#include <cstring>
#include <fstream>

#include <boost/filesystem.hpp>

using namespace sc_core;
using boost::filesystem::path;

const sc_time MEM_READ_LATENCY(3, SC_NS);
const sc_time MEM_WRITE_LATENCY(3, SC_NS);

const std::string FILE_BLOB = "test/blob";

template <bool READONLY = false, bool LOAD_BLOB = false, uint64_t _MEM_SIZE=0x1000>
class MemoryTester : public Test {
protected:
    static const uint64_t MEM_SIZE = _MEM_SIZE;

    ComponentBase *mem;
    SlaveTester<> tst;

    MemoryTester(sc_module_name n, ConfigManager &c) : Test(n, c), tst("slave-tester", c)
    {
        std::stringstream yml;

        yml << "size: " << MEM_SIZE << "\n";
        yml << "readonly: " << READONLY << "\n";

        if (LOAD_BLOB) {
            path blob_path(RABBITS_GET_TEST_DIR());
            blob_path /= FILE_BLOB;
            yml << "file-blob: " << blob_path.string() << "\n";
        }

        mem = create_component_by_implem("generic-memory", yml.str());

        mem->get_port("bus").connect(tst.get_port("bus"));
    }

    uint64_t load_blob(std::vector<uint8_t> &blob) {
        path blob_path(RABBITS_GET_TEST_DIR());
        blob_path /= FILE_BLOB;

        LOG(APP, DBG) << "Trying to load blob file " << blob_path.string() << "\n";

        std::ifstream f(blob_path.string().c_str(), std::ifstream::binary);
        RABBITS_TEST_ASSERT(f.good());

        f.unsetf(std::ios::skipws);

        f.seekg(0, std::ios::end);
        std::streampos file_size = f.tellg();
        f.seekg(0, std::ios::beg);

        blob.resize(file_size);

        std::copy(std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>(), blob.begin());

        return file_size;
    }

    void mute_logger() {
        get_app_logger().mute();
        get_sim_logger().mute();
        mem->get_logger(LogContext::APP).mute();
        mem->get_logger(LogContext::SIM).mute();
        tst.get_logger(LogContext::APP).mute();
        tst.get_logger(LogContext::SIM).mute();
    }

    void unmute_logger() {
        tst.get_logger(LogContext::APP).unmute();
        tst.get_logger(LogContext::SIM).unmute();
        mem->get_logger(LogContext::APP).unmute();
        mem->get_logger(LogContext::SIM).unmute();
        get_app_logger().unmute();
        get_sim_logger().unmute();
    }

public:
    ~MemoryTester() {
        delete mem;
        mem = NULL;
    }
};


RABBITS_UNIT_TEST(memory_write_io, MemoryTester<>)
{
    tst.bus_write_u32(0x0, 0xdecacafe);

    RABBITS_TEST_ASSERT(tst.last_access_succeeded());
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_WRITE_LATENCY);

    RABBITS_TEST_ASSERT(tst.debug_read_u32_nofail(0x0) == 0xdecacafe);
    RABBITS_TEST_ASSERT_TIME_DELTA(SC_ZERO_TIME); /* Debug access should not have
                                                     side effects on simulation time */
}

RABBITS_UNIT_TEST(memory_read_io, MemoryTester<>)
{
    tst.debug_write_u32_nofail(0x0, 0xdecacafe);

    RABBITS_TEST_ASSERT_TIME_DELTA(SC_ZERO_TIME); /* Debug access should not have
                                                     side effects on simulation time */

    RABBITS_TEST_ASSERT(tst.bus_read_u32(0x0) == 0xdecacafe);
    RABBITS_TEST_ASSERT(tst.last_access_succeeded());
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_READ_LATENCY);
}

RABBITS_UNIT_TEST(memory_dmi, MemoryTester<>)
{
    DmiInfo dmi;

    RABBITS_TEST_ASSERT(tst.get_dmi_info(dmi));
    RABBITS_TEST_ASSERT(dmi.is_read_write_allowed());
    RABBITS_TEST_ASSERT_EQ(dmi.range, AddressRange(0, MEM_SIZE));
    RABBITS_TEST_ASSERT_EQ(dmi.read_latency, MEM_READ_LATENCY);
    RABBITS_TEST_ASSERT_EQ(dmi.write_latency, MEM_WRITE_LATENCY);

    uint32_t * mem = reinterpret_cast<uint32_t*>(dmi.ptr);

    mem[0x20] = 0xbeef5a7a;
    RABBITS_TEST_ASSERT_EQ(
        tst.debug_read_u32_nofail(0x20 * sizeof(uint32_t)),
        0xbeef5a7a);

    tst.debug_write_u32_nofail(0x40 * sizeof(uint32_t), 0xdeadbeef);
    RABBITS_TEST_ASSERT_EQ(mem[0x40], 0xdeadbeef);
}

RABBITS_UNIT_TEST(memory_access_outbound, MemoryTester<>)
{
    mute_logger(); /* Outbound accesses generate errors */
    tst.bus_write_u32(MEM_SIZE, 0xdeadbaba);
    unmute_logger();

    RABBITS_TEST_ASSERT(tst.last_access_failed());
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_WRITE_LATENCY);

    mute_logger();
    tst.bus_read_u32(MEM_SIZE);
    unmute_logger();

    RABBITS_TEST_ASSERT(tst.last_access_failed());
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_WRITE_LATENCY);
}

RABBITS_UNIT_TEST(memory_readonly, MemoryTester<true>)
{
    mute_logger();
    tst.bus_write_u32(0x0, 0xdada7070);
    unmute_logger();

    RABBITS_TEST_ASSERT(tst.last_access_failed());
    RABBITS_TEST_ASSERT_NE(tst.debug_read_u32_nofail(0x0), 0xdada7070);
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_WRITE_LATENCY);
}

RABBITS_UNIT_TEST(memory_readonly_dmi, MemoryTester<true>)
{
    DmiInfo dmi;

    RABBITS_TEST_ASSERT(tst.get_dmi_info(dmi));
    RABBITS_TEST_ASSERT(dmi.read_allowed);
    RABBITS_TEST_ASSERT(!dmi.write_allowed);
}


#define COMMA ,
RABBITS_UNIT_TEST(memory_load_blob_plenty, MemoryTester<false COMMA true>)
{
    DmiInfo dmi;

    RABBITS_TEST_ASSERT(tst.get_dmi_info(dmi));

    std::vector<uint8_t> blob;
    uint64_t file_size = load_blob(blob);

    RABBITS_TEST_ASSERT_EQ(std::memcmp(&blob[0], dmi.ptr, file_size), 0);
}


RABBITS_UNIT_TEST(memory_load_blob_fit, MemoryTester<false COMMA true COMMA 1024>)
{
    DmiInfo dmi;

    RABBITS_TEST_ASSERT(tst.get_dmi_info(dmi));

    std::vector<uint8_t> blob;
    uint64_t file_size = load_blob(blob);

    RABBITS_TEST_ASSERT_EQ(std::memcmp(&blob[0], dmi.ptr, file_size), 0);
}

RABBITS_UNIT_TEST(memory_load_blob_trunc, MemoryTester<false COMMA true COMMA 512>)
{
    DmiInfo dmi;

    RABBITS_TEST_ASSERT(tst.get_dmi_info(dmi));

    std::vector<uint8_t> blob;
    load_blob(blob);

    RABBITS_TEST_ASSERT_EQ(std::memcmp(&blob[0], dmi.ptr, MEM_SIZE), 0);
}
