#include <catch2/catch_test_macros.hpp>
#include "memory.hpp"

TEST_CASE("Memory: load and read back", "[memory]")
{
    Memory mem(0x80000000, 0x1000);

    std::vector<uint8_t> program = {0x01, 0x02, 0x03, 0x04};
    REQUIRE(mem.load(program, 0x80000000));

    uint8_t  b;
    uint16_t h;
    uint32_t w;

    REQUIRE(mem.read8 (0x80000000, b) == memFault::none);
    REQUIRE(b == 0x01);

    REQUIRE(mem.read16(0x80000000, h) == memFault::none);
    REQUIRE(h == 0x0201);   // little-endian

    REQUIRE(mem.read32(0x80000000, w) == memFault::none);
    REQUIRE(w == 0x04030201);
}

TEST_CASE("Memory: out of bounds", "[memory]")
{
    Memory mem(0x80000000, 0x1000);

    uint32_t w;
    REQUIRE(mem.read32(0x7FFFFFFC, w) == memFault::outOfBounds);  // before base
    REQUIRE(mem.read32(0x80001000, w) == memFault::outOfBounds);  // after end
}

TEST_CASE("Memory: misaligned access", "[memory]")
{
    Memory mem(0x80000000, 0x1000);

    uint16_t h;
    uint32_t w;
    REQUIRE(mem.read16(0x80000001, h) == memFault::misaligned);
    REQUIRE(mem.read32(0x80000002, w) == memFault::misaligned);
}

TEST_CASE("Memory: load does not fit", "[memory]")
{
    Memory mem(0x80000000, 0x10);  // only 16 bytes

    std::vector<uint8_t> big(0x100, 0xFF);  // 256 bytes
    REQUIRE_FALSE(mem.load(big, 0x80000000));
}