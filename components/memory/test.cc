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

using namespace sc_core;

const uint64_t MEM_SIZE = 0x1000;
const sc_time MEM_READ_LATENCY(3, SC_NS);
const sc_time MEM_WRITE_LATENCY(3, SC_NS);

template <bool READONLY = false>
class MemoryTester : public Test {
protected:
    ComponentBase *mem;
    SlaveTester<> tst;


    MemoryTester(sc_module_name n) : Test(n), tst("slave-tester")
    {
        Slave *s = NULL;
        std::stringstream yml;

        yml << "size: " << MEM_SIZE << "\n";
        yml << "readonly: " << READONLY << "\n";

        mem = create_component_by_name("generic-memory", yml.str());

        s = dynamic_cast<Slave*>(mem);

        RABBITS_TEST_ASSERT(s != NULL);

        tst.connect(*s);
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
    tlm::tlm_dmi dmi;

    RABBITS_TEST_ASSERT(tst.get_dmi_info(dmi));
    RABBITS_TEST_ASSERT(dmi.is_read_write_allowed());
    RABBITS_TEST_ASSERT_EQ(dmi.get_start_address(), (uint64_t)0);
    RABBITS_TEST_ASSERT_EQ(dmi.get_end_address(), MEM_SIZE - 1);
    RABBITS_TEST_ASSERT_EQ(dmi.get_read_latency(), MEM_READ_LATENCY);
    RABBITS_TEST_ASSERT_EQ(dmi.get_write_latency(), MEM_WRITE_LATENCY);

    uint32_t * mem = reinterpret_cast<uint32_t*>(dmi.get_dmi_ptr());

    mem[0x20] = 0xbeef5a7a;
    RABBITS_TEST_ASSERT_EQ(
        tst.debug_read_u32_nofail(0x20 * sizeof(uint32_t)),
        0xbeef5a7a);

    tst.debug_write_u32_nofail(0x40 * sizeof(uint32_t), 0xdeadbeef);
    RABBITS_TEST_ASSERT_EQ(mem[0x40], 0xdeadbeef);
}

RABBITS_UNIT_TEST(memory_access_outbound, MemoryTester<>)
{
    Logger::get().mute(); /* Outbound accesses generate errors */
    tst.bus_write_u32(MEM_SIZE, 0xdeadbaba);
    Logger::get().unmute();

    RABBITS_TEST_ASSERT(tst.last_access_failed());
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_WRITE_LATENCY);

    Logger::get().mute();
    tst.bus_read_u32(MEM_SIZE);
    Logger::get().unmute();

    RABBITS_TEST_ASSERT(tst.last_access_failed());
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_WRITE_LATENCY);
}

RABBITS_UNIT_TEST(memory_readonly, MemoryTester<true>)
{
    Logger::get().mute();
    tst.bus_write_u32(0x0, 0xdada7070);
    Logger::get().unmute();

    RABBITS_TEST_ASSERT(tst.last_access_failed());
    RABBITS_TEST_ASSERT_NE(tst.debug_read_u32_nofail(0x0), 0xdada7070);
    RABBITS_TEST_ASSERT_TIME_DELTA(MEM_WRITE_LATENCY);
}

RABBITS_UNIT_TEST(memory_readonly_dmi, MemoryTester<true>)
{
    tlm::tlm_dmi dmi;

    RABBITS_TEST_ASSERT(tst.get_dmi_info(dmi));
    RABBITS_TEST_ASSERT(dmi.is_read_allowed());
    RABBITS_TEST_ASSERT(!dmi.is_write_allowed());
}
