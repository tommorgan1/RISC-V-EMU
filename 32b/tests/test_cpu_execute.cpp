#include <catch2/catch_test_macros.hpp>
#include "cpu.hpp"

// -------------------------------------------------------------------------
// Helper — builds a CPU, writes a single instruction at base, runs one step
// Returns the CPU so tests can inspect register state
// -------------------------------------------------------------------------

static CPU run_one(uint32_t instruction)
{
    CPU cpu(0x80000000, 0x1000);
    std::vector<uint8_t> prog = {
        static_cast<uint8_t>(instruction),
        static_cast<uint8_t>(instruction >> 8),
        static_cast<uint8_t>(instruction >> 16),
        static_cast<uint8_t>(instruction >> 24),
        0x73, 0x00, 0x00, 0x00  // ECALL — halts execution cleanly
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}
    return cpu;
}

// -------------------------------------------------------------------------
// R-type — ADD
// ADD x3, x1, x2:  rd=x3, rs1=x1, rs2=x2, funct3=0x0, funct7=0x00
// -------------------------------------------------------------------------

TEST_CASE("Execute R: ADD", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // ADD x3, x1, x2 — 0x002081B3
    std::vector<uint8_t> prog = {
        0xB3, 0x01, 0x20, 0x00,   // add x3, x1, x2
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);

    // Manually seed registers via a sequence of ADDI instructions
    // ADDI x1, x0, 10  →  0x00A00093
    // ADDI x2, x0, 20  →  0x01400113
    // Can't directly set regs from outside — use a longer program
    std::vector<uint8_t> full = {
        0x93, 0x00, 0xA0, 0x00,   // addi x1, x0, 10
        0x13, 0x01, 0x40, 0x01,   // addi x2, x0, 20
        0xB3, 0x81, 0x20, 0x00,   // add  x3, x1, x2
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(full, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(1) == 10);
    REQUIRE(cpu.reg(2) == 20);
    REQUIRE(cpu.reg(3) == 30);
}

TEST_CASE("Execute R: ADD overflow wraps", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // ADDI x1, x0, -1  →  x1 = 0xFFFFFFFF
    // ADDI x2, x0,  1  →  x2 = 1
    // ADD  x3, x1, x2  →  x3 = 0x00000000 (wraps)
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xF0, 0xFF,   // addi x1, x0, -1
        0x13, 0x01, 0x10, 0x00,   // addi x2, x0, 1
        0xB3, 0x81, 0x20, 0x00,   // add  x3, x1, x2  
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(3) == 0x00000000);
}

TEST_CASE("Execute R: SUB", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, 30
    // addi x2, x0, 10
    // sub  x3, x1, x2  →  x3 = 20
    // sub  x3 = 0x40208_1B3 — funct7=0x20
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xE0, 0x01,   // addi x1, x0, 30
        0x13, 0x01, 0xA0, 0x00,   // addi x2, x0, 10
        0xB3, 0x81, 0x20, 0x40,   // sub  x3, x1, x2
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(3) == 20);
}

TEST_CASE("Execute R: SUB produces negative (wraps to unsigned)", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, 5
    // addi x2, x0, 10
    // sub  x3, x1, x2  →  x3 = 0xFFFFFFFB (-5 as unsigned)
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0x50, 0x00,   // addi x1, x0, 5
        0x13, 0x01, 0xA0, 0x00,   // addi x2, x0, 10
        0xB3, 0x81, 0x20, 0x40,   // sub  x3, x1, x2
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(3) == 0xFFFFFFFB);
}

TEST_CASE("Execute R: SLT signed", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, -1   →  x1 = 0xFFFFFFFF (-1 signed)
    // addi x2, x0,  1   →  x2 = 1
    // slt  x3, x1, x2   →  x3 = 1 (−1 < 1 signed)
    // SLT: funct3=0x2, funct7=0x00  →  0x0020A1B3
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xF0, 0xFF,   // addi x1, x0, -1
        0x13, 0x01, 0x10, 0x00,   // addi x2, x0, 1
        0xB3, 0x21, 0x20, 0x00,   // slt  x3, x1, x2
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(3) == 1);
}

TEST_CASE("Execute R: SLTU unsigned", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, -1   →  x1 = 0xFFFFFFFF (large unsigned)
    // addi x2, x0,  1   →  x2 = 1
    // sltu x3, x1, x2   →  x3 = 0 (0xFFFFFFFF > 1 unsigned)
    // SLTU: funct3=0x3, funct7=0x00  →  0x0020B1B3
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xF0, 0xFF,   // addi x1, x0, -1
        0x13, 0x01, 0x10, 0x00,   // addi x2, x0, 1
        0xB3, 0xB1, 0x20, 0x00,   // sltu x3, x1, x2  
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(3) == 0);
}

TEST_CASE("Execute R: SRL vs SRA", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, -8   →  x1 = 0xFFFFFFF8
    // addi x2, x0,  1   →  x2 = 1
    // srl  x3, x1, x2   →  x3 = 0x7FFFFFFC  (logical, zero-fill)
    // sra  x4, x1, x2   →  x4 = 0xFFFFFFFC  (arithmetic, sign-fill)
    //
    // SRL: funct3=0x5, funct7=0x00  rd=x3  →  0x002051B3
    // SRA: funct3=0x5, funct7=0x20  rd=x4  →  0x40205233
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0x80, 0xFF,   // addi x1, x0, -8
        0x13, 0x01, 0x10, 0x00,   // addi x2, x0, 1
        0xB3, 0xD1, 0x20, 0x00,   // srl  x3, x1, x2
        0x33, 0xD2, 0x20, 0x40,   // sra  x4, x1, x2
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(3) == 0x7FFFFFFC);
    REQUIRE(cpu.reg(4) == 0xFFFFFFFC);
}

TEST_CASE("Execute R: x0 always zero", "[cpu][R]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, 42
    // add  x0, x1, x1   →  x0 must remain 0
    // ADD rd=x0: 0x00108033
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xA0, 0x02,   // addi x1, x0, 42
        0x33, 0x80, 0x10, 0x00,   // add  x0, x1, x1
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(0) == 0);
}

TEST_CASE("Execute I: ADDI positive", "[cpu][I]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, 42  →  x1 = 42
    // 0x02A00093
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xA0, 0x02,
        0x73, 0x00, 0x00, 0x00
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(1) == 42);
}

TEST_CASE("Execute I: ADDI negative immediate", "[cpu][I]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, -1  →  x1 = 0xFFFFFFFF
    // 0xFFF00093
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xF0, 0xFF,
        0x73, 0x00, 0x00, 0x00
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(1) == 0xFFFFFFFF);
}

TEST_CASE("Execute I: ADDI chain", "[cpu][I]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, 10   →  x1 = 10
    // addi x1, x1, 10   →  x1 = 20
    // addi x1, x1, 10   →  x1 = 30
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xA0, 0x00,   // addi x1, x0, 10
        0x93, 0x80, 0xA0, 0x00,   // addi x1, x1, 10
        0x93, 0x80, 0xA0, 0x00,   // addi x1, x1, 10
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(1) == 30);
}

TEST_CASE("Execute I: SLTI signed", "[cpu][I]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, -5   →  x1 = 0xFFFFFFFB
    // slti x2, x1, 0    →  x2 = 1  (-5 < 0 signed)
    // slti x3, x1, -10  →  x3 = 0  (-5 > -10 signed)
    //
    // SLTI x2, x1, 0:   imm=0,   funct3=2, rd=x2  →  0x00002113  (but rs1=x1)
    // 0x00002113 = imm[11:0]=0, rs1=x0... let me be precise
    // slti rd=x2 rs1=x1 imm=0:  0x0000A113
    // slti rd=x3 rs1=x1 imm=-10: imm=-10=0xFF6, 0xFF608193
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xB0, 0xFF,   // addi x1, x0, -5
        0x13, 0xA1, 0x00, 0x00,   // slti x2, x1, 0 
        0x93, 0xA1, 0x60, 0xFF,   // slti x3, x1, -10 
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(2) == 1);
    REQUIRE(cpu.reg(3) == 0);
}

TEST_CASE("Execute I: SLTIU unsigned", "[cpu][I]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, -1   →  x1 = 0xFFFFFFFF
    // sltiu x2, x1, 1   →  x2 = 0 (0xFFFFFFFF > 1 unsigned)
    // SLTIU: funct3=0x3 rd=x2 rs1=x1 imm=1  →  0x00103113... 
    // Let me encode: imm=1 rs1=x1(00001) funct3=011 rd=x2(00010) opcode=0010011
    // = 000000000001_00001_011_00010_0010011 = 0x00103113 -- wait
    // bits: [31:20]=000000000001, [19:15]=00001, [14:12]=011, [11:7]=00010, [6:0]=0010011
    // = 0x00103113... no: 0x001_00001_011_00010_0010011
    // = 0x00100_0b13... let me just be careful:
    // 0000_0000_0001 | 0_0001 | 011 | 0_0010 | 001_0011
    // = 0x00103113 -- no that's wrong. Let me re-encode.
    // imm=1=0x001, rs1=1=0x01, funct3=3=0x3, rd=2=0x2, opcode=0x13
    // [31:20]=0x001=000000000001, [19:15]=00001, [14:12]=011, [11:7]=00010, [6:0]=0010011
    // Assembling: 000000000001_00001_011_00010_0010011
    //           = 0000_0000_0001_0000_1011_0001_0001_0011
    //           = 0x00 10 B1 13... 
    //           = 0x00 1 0 B 1 1 3
    //           Hex: 0x0010B113
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xF0, 0xFF,   // addi  x1, x0, -1
        0x13, 0xB1, 0x10, 0x00,   // sltiu x2, x1, 1
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(2) == 0);
}

TEST_CASE("Execute I: XORI, ORI, ANDI", "[cpu][I]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, 0xFF  →  x1 = 255
    // xori x2, x1, 0x0F  →  x2 = 0xF0
    // ori  x3, x1, 0x100 →  x3 = 0x1FF  (but imm is 12-bit signed, 0x100=256)
    // andi x4, x1, 0x0F  →  x4 = 0x0F
    //
    // Encoding each carefully:
    // addi x1, x0, 255:  imm=0xFF rs1=0 funct3=0 rd=1 op=0x13 → 0x0FF00093
    // xori x2, x1, 15:   imm=0xF  rs1=1 funct3=4 rd=2 op=0x13 → 0x00F04113 -- 
    //   0000_0000_1111_00001_100_00010_0010011 = 0x00F04113
    // andi x4, x1, 15:   imm=0xF  rs1=1 funct3=7 rd=4 op=0x13 
    //   0000_0000_1111_00001_111_00100_0010011 = 0x00F07213
    // ori  x3, x1, 256:  imm=0x100 rs1=1 funct3=6 rd=3 op=0x13
    //   0001_0000_0000_00001_110_00011_0010011 = 0x1000_61_93... 
    //   = 0x100061B3... let me just use a simpler value
    // ori x3, x1, 0x0F:  imm=0xF rs1=1 funct3=6 rd=3 → 0x00F061B3
    //   0000_0000_1111_00001_110_00011_0010011 = 0x00F06193
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0xF0, 0x0F,   // addi x1, x0, 255
        0x13, 0xC1, 0xF0, 0x00,   // xori x2, x1, 15  
        0x93, 0xE1, 0xF0, 0x00,   // ori  x3, x1, 15  
        0x13, 0xF2, 0xF0, 0x00,   // andi x4, x1, 15  
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(2) == 0xF0);
    REQUIRE(cpu.reg(3) == 0xFF);
    REQUIRE(cpu.reg(4) == 0x0F);
}

TEST_CASE("Execute I: SLLI, SRLI, SRAI", "[cpu][I]")
{
    CPU cpu(0x80000000, 0x1000);

    // addi x1, x0, -8   →  x1 = 0xFFFFFFF8
    // slli x2, x1, 2    →  x2 = 0xFFFFFFE0
    // srli x3, x1, 2    →  x3 = 0x3FFFFFFE  (logical)
    // srai x4, x1, 2    →  x4 = 0xFFFFFFFE  (arithmetic)
    //
    // SLLI rd=x2 rs1=x1 shamt=2: funct7=0x00 funct3=0x1
    //   0000000_00010_00001_001_00010_0010011 = 0x00209113
    // SRLI rd=x3 rs1=x1 shamt=2: funct7=0x00 funct3=0x5
    //   0000000_00010_00001_101_00011_0010011 = 0x0020D193
    // SRAI rd=x4 rs1=x1 shamt=2: funct7=0x20 funct3=0x5
    //   0100000_00010_00001_101_00100_0010011 = 0x4020D213
    std::vector<uint8_t> prog = {
        0x93, 0x00, 0x80, 0xFF,   // addi x1, x0, -8
        0x13, 0x91, 0x20, 0x00,   // slli x2, x1, 2
        0x93, 0xD1, 0x20, 0x00,   // srli x3, x1, 2
        0x13, 0xD2, 0x20, 0x40,   // srai x4, x1, 2  
        0x73, 0x00, 0x00, 0x00    // ecall
    };
    cpu.load(prog, 0x80000000);
    try { cpu.run(); } catch (...) {}

    REQUIRE(cpu.reg(2) == 0xFFFFFFE0);
    REQUIRE(cpu.reg(3) == 0x3FFFFFFE);
    REQUIRE(cpu.reg(4) == 0xFFFFFFFE);
}