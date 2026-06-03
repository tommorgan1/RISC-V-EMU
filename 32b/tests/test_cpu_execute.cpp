#include <catch2/catch_test_macros.hpp>

#include "cpu.hpp"

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

// Load a single instruction followed by ECALL, run it, and return the CPU.
// Suitable for instructions that source only from x0 (e.g. ADDI x_rd, x0, imm).
static CPU run_one(uint32_t instruction)
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    static_cast<uint8_t>(instruction),
	    static_cast<uint8_t>(instruction >> 8),
	    static_cast<uint8_t>(instruction >> 16),
	    static_cast<uint8_t>(instruction >> 24),
	    0x73,
	    0x00,
	    0x00,
	    0x00  // ECALL
	};
	cpu.load(prog, 0x80000000);
	try
	{
		cpu.run();
	}
	catch (...)
	{
	}
	return cpu;
}

// Run until ECALL.  Re-throws any exception that is NOT "ECALL", so real
// faults (illegal instruction, memory fault, etc.) are never silently
// swallowed.
static void run_to_ecall(CPU &cpu)
{
	try
	{
		cpu.run();
		FAIL("Expected ECALL halt but CPU ran off the end of the program");
	}
	catch (const std::runtime_error &e)
	{
		if (std::string(e.what()) != "ECALL")
		{
			throw;
		}
	}
}

// -------------------------------------------------------------------------
// R-type
// -------------------------------------------------------------------------

TEST_CASE("Execute R: ADD", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xA0, 0x00,  // addi x1, x0, 10
	    0x13, 0x01, 0x40, 0x01,  // addi x2, x0, 20
	    0xB3, 0x81, 0x20, 0x00,  // add  x3, x1, x2
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(1) == 10);
	REQUIRE(cpu.readReg(2) == 20);
	REQUIRE(cpu.readReg(3) == 30);
}

TEST_CASE("Execute R: ADD overflow wraps", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xF0, 0xFF,  // addi x1, x0, -1   → x1 = 0xFFFFFFFF
	    0x13, 0x01, 0x10, 0x00,  // addi x2, x0, 1
	    0xB3, 0x81, 0x20, 0x00,  // add  x3, x1, x2   → x3 = 0 (wraps)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 0x00000000);
}

TEST_CASE("Execute R: SUB", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xE0, 0x01,  // addi x1, x0, 30
	    0x13, 0x01, 0xA0, 0x00,  // addi x2, x0, 10
	    0xB3, 0x81, 0x20, 0x40,  // sub  x3, x1, x2   → x3 = 20
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 20);
}

TEST_CASE("Execute R: SUB produces negative (wraps to unsigned)", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0x50, 0x00,  // addi x1, x0, 5
	    0x13, 0x01, 0xA0, 0x00,  // addi x2, x0, 10
	    0xB3, 0x81, 0x20, 0x40,  // sub  x3, x1, x2   → x3 = 0xFFFFFFFB
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 0xFFFFFFFB);
}

TEST_CASE("Execute R: SLT signed", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xF0, 0xFF,  // addi x1, x0, -1
	    0x13, 0x01, 0x10, 0x00,  // addi x2, x0, 1
	    0xB3, 0x21, 0x20, 0x00,  // slt  x3, x1, x2   → x3 = 1 (-1 < 1 signed)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 1);
}

TEST_CASE("Execute R: SLTU unsigned", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xF0, 0xFF,  // addi x1, x0, -1   → x1 = 0xFFFFFFFF
	    0x13, 0x01, 0x10, 0x00,  // addi x2, x0, 1
	    0xB3, 0xB1, 0x20, 0x00,  // sltu x3, x1, x2   → x3 = 0 (0xFFFFFFFF > 1)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 0);
}

TEST_CASE("Execute R: SRL vs SRA", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0x80, 0xFF,  // addi x1, x0, -8   → x1 = 0xFFFFFFF8
	    0x13, 0x01, 0x10, 0x00,  // addi x2, x0, 1
	    0xB3, 0xD1, 0x20, 0x00,  // srl  x3, x1, x2   → x3 = 0x7FFFFFFC (logical)
	    0x33, 0xD2, 0x20, 0x40,  // sra  x4, x1, x2   → x4 = 0xFFFFFFFC (arithmetic)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 0x7FFFFFFC);
	REQUIRE(cpu.readReg(4) == 0xFFFFFFFC);
}

TEST_CASE("Execute R: x0 always zero", "[cpu][R]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xA0, 0x02,  // addi x1, x0, 42
	    0x33, 0x80, 0x10, 0x00,  // add  x0, x1, x1   → x0 must stay 0
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(0) == 0);
}

// -------------------------------------------------------------------------
// I-type (ALU)
// -------------------------------------------------------------------------

// addi x1, x0, 42  =  0x02A00093
TEST_CASE("Execute I: ADDI positive", "[cpu][I]")
{
	REQUIRE(run_one(0x02A00093).readReg(1) == 42);
}

// addi x1, x0, -1  =  0xFFF00093
TEST_CASE("Execute I: ADDI negative immediate", "[cpu][I]")
{
	REQUIRE(run_one(0xFFF00093).readReg(1) == 0xFFFFFFFF);
}

TEST_CASE("Execute I: ADDI chain", "[cpu][I]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xA0, 0x00,  // addi x1, x0, 10
	    0x93, 0x80, 0xA0, 0x00,  // addi x1, x1, 10
	    0x93, 0x80, 0xA0, 0x00,  // addi x1, x1, 10   → x1 = 30
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(1) == 30);
}

TEST_CASE("Execute I: SLTI signed", "[cpu][I]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xB0, 0xFF,  // addi x1, x0, -5
	    0x13, 0xA1, 0x00, 0x00,  // slti x2, x1, 0    → x2 = 1  (-5 < 0)
	    0x93, 0xA1, 0x60, 0xFF,  // slti x3, x1, -10  → x3 = 0  (-5 > -10)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(2) == 1);
	REQUIRE(cpu.readReg(3) == 0);
}

TEST_CASE("Execute I: SLTIU unsigned", "[cpu][I]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xF0, 0xFF,  // addi  x1, x0, -1  → x1 = 0xFFFFFFFF
	    0x13, 0xB1, 0x10, 0x00,  // sltiu x2, x1, 1   → x2 = 0 (0xFFFFFFFF > 1)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(2) == 0);
}

TEST_CASE("Execute I: XORI, ORI, ANDI", "[cpu][I]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0xF0, 0x0F,  // addi x1, x0, 255
	    0x13, 0xC1, 0xF0, 0x00,  // xori x2, x1, 15   → x2 = 0xF0
	    0x93, 0xE1, 0xF0, 0x00,  // ori  x3, x1, 15   → x3 = 0xFF
	    0x13, 0xF2, 0xF0, 0x00,  // andi x4, x1, 15   → x4 = 0x0F
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(2) == 0xF0);
	REQUIRE(cpu.readReg(3) == 0xFF);
	REQUIRE(cpu.readReg(4) == 0x0F);
}

TEST_CASE("Execute I: SLLI, SRLI, SRAI", "[cpu][I]")
{
	CPU                  cpu(0x80000000, 0x1000);
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0x80, 0xFF,  // addi x1, x0, -8   → x1 = 0xFFFFFFF8
	    0x13, 0x91, 0x20, 0x00,  // slli x2, x1, 2    → x2 = 0xFFFFFFE0
	    0x93, 0xD1, 0x20, 0x00,  // srli x3, x1, 2    → x3 = 0x3FFFFFFE
	    0x13, 0xD2, 0x20, 0x40,  // srai x4, x1, 2    → x4 = 0xFFFFFFFE
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(2) == 0xFFFFFFE0);
	REQUIRE(cpu.readReg(3) == 0x3FFFFFFE);
	REQUIRE(cpu.readReg(4) == 0xFFFFFFFE);
}

// -------------------------------------------------------------------------
// S-type / load (roundtrip)
// -------------------------------------------------------------------------

TEST_CASE("Execute S: SW/LW roundtrip", "[cpu][S]")
{
	CPU cpu(0x80000000, 0x1000);
	// Store 0x42 to mem[base + 32], then reload into x3.
	// Offset 32 is safely past the 5-instruction program (20 bytes).
	//
	//   lui  x2, 0x80000   → x2 = 0x80000000        (0x80000137)
	//   addi x1, x0, 0x42  → x1 = 0x42              (0x04200093)
	//   sw   x1, 32(x2)                              (0x02112023)
	//   lw   x3, 32(x2)    → x3 = 0x42              (0x02012183)
	std::vector<uint8_t> prog = {
	    0x37, 0x01, 0x00, 0x80,  // lui  x2, 0x80000
	    0x93, 0x00, 0x20, 0x04,  // addi x1, x0, 0x42
	    0x23, 0x20, 0x11, 0x02,  // sw   x1, 32(x2)
	    0x83, 0x21, 0x01, 0x02,  // lw   x3, 32(x2)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 0x42);
}

// -------------------------------------------------------------------------
// B-type
// -------------------------------------------------------------------------

TEST_CASE("Execute B: BEQ taken", "[cpu][B]")
{
	CPU cpu(0x80000000, 0x1000);
	// beq x1, x2, +8 skips the addi x3=42 at offset 12 and lands on
	// addi x3=99 at offset 16.
	//
	//   addi x3, x0, 42  → 0x02A00193  (rd=x3, not x1)
	//   addi x3, x0, 99  → 0x06300193
	std::vector<uint8_t> prog = {
	    0x93, 0x00, 0x10, 0x00,  // addi x1, x0, 1
	    0x13, 0x01, 0x10, 0x00,  // addi x2, x0, 1
	    0x63, 0x04, 0x21, 0x01,  // beq  x1, x2, +8   → branch taken
	    0x93, 0x01, 0xA0, 0x02,  // addi x3, x0, 42   (skipped)
	    0x93, 0x01, 0x30, 0x06,  // addi x3, x0, 99   (executed)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(3) == 99);
}

// -------------------------------------------------------------------------
// U-type
// -------------------------------------------------------------------------

// lui x1, 0x12345  =  0x123450B7  →  result = 0x12345000
TEST_CASE("Execute U: LUI", "[cpu][U]")
{
	REQUIRE(run_one(0x123450B7).readReg(1) == 0x12345000);
}

// auipc x1, 1 at PC = 0x80000000  →  result = 0x80000000 + 0x1000 = 0x80001000
// auipc x1, 1  =  0x00001097
TEST_CASE("Execute U: AUIPC", "[cpu][U]")
{
	REQUIRE(run_one(0x00001097).readReg(1) == 0x80001000);
}

// -------------------------------------------------------------------------
// J-type
// -------------------------------------------------------------------------

TEST_CASE("Execute J: JAL", "[cpu][J]")
{
	CPU cpu(0x80000000, 0x1000);
	// jal x1, +8 skips addi x2=42 at offset 4, lands on addi x2=99 at offset 8.
	//
	//   jal x1, +8  =  0x008000EF  (NOT 0x010000EF which encodes +16)
	//   addi x2, x0, 99  =  0x06300113  (imm=0x63=99, NOT 0x6C=108)
	std::vector<uint8_t> prog = {
	    0xEF, 0x00, 0x80, 0x00,  // jal  x1, +8        → x1=0x80000004, PC→0x80000008
	    0x13, 0x01, 0xA0, 0x02,  // addi x2, x0, 42    (skipped)
	    0x13, 0x01, 0x30, 0x06,  // addi x2, x0, 99    (executed)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(1) == 0x80000004);
	REQUIRE(cpu.readReg(2) == 99);
}

TEST_CASE("Execute J: JALR", "[cpu][JALR]")
{
	CPU cpu(0x80000000, 0x1000);
	// Build an absolute jump target using auipc + addi so the address lies
	// within the memory region — an immediate of 8 is not a valid address.
	//
	//   auipc x1, 0     @ offset 0   → x1 = 0x80000000   (0x00000097)
	//   addi  x1, x1, 16             → x1 = 0x80000010   (0x01008093)
	//   jalr  x2, x1, 0 @ offset 8  → PC = 0x80000010,
	//                                  x2 = 0x8000000C   (0x00008167)
	//   addi  x3, x0, 42 @ offset 12  (skipped)          (0x02A00193)
	//   addi  x3, x0, 99 @ offset 16  (executed)         (0x06300193)
	std::vector<uint8_t> prog = {
	    0x97, 0x00, 0x00, 0x00,  // auipc x1, 0
	    0x93, 0x80, 0x00, 0x01,  // addi  x1, x1, 16
	    0x67, 0x81, 0x00, 0x00,  // jalr  x2, x1, 0
	    0x93, 0x01, 0xA0, 0x02,  // addi  x3, x0, 42    (skipped)
	    0x93, 0x01, 0x30, 0x06,  // addi  x3, x0, 99    (executed)
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(2) == 0x8000000C);
	REQUIRE(cpu.readReg(3) == 99);
}

// -------------------------------------------------------------------------
// SYS: ECALL / EBREAK
// Each gets its own CPU — reusing one CPU across both tests is wrong because
// the PC is left past the ECALL after the first run() throws.
// -------------------------------------------------------------------------

TEST_CASE("Execute SYS: ECALL", "[cpu][SYS]")
{
	CPU cpu(0x80000000, 0x1000);
	// ecall  =  0x00000073
	std::vector<uint8_t> prog = {0x73, 0x00, 0x00, 0x00};
	cpu.load(prog, 0x80000000);
	try
	{
		cpu.run();
		FAIL("Expected ECALL throw");
	}
	catch (const std::runtime_error &e)
	{
		REQUIRE(std::string(e.what()) == "ECALL");
	}
}

TEST_CASE("Execute SYS: EBREAK", "[cpu][SYS]")
{
	CPU cpu(0x80000000, 0x1000);
	// ebreak  =  0x00100073  (NOT 0x00001073 which is CSRRW)
	std::vector<uint8_t> prog = {0x73, 0x00, 0x10, 0x00};
	cpu.load(prog, 0x80000000);
	try
	{
		cpu.run();
		FAIL("Expected EBREAK throw");
	}
	catch (const std::runtime_error &e)
	{
		REQUIRE(std::string(e.what()) == "EBREAK");
	}
}

// -------------------------------------------------------------------------
// SYS: CSR read-modify-write sequence
// -------------------------------------------------------------------------

TEST_CASE("Execute SYS: CSR read-modify-write sequence", "[cpu][SYS]")
{
	CPU cpu(0x80000000, 0x1000);
	// All four operations target mstatus (CSR 0x300).
	//
	//   csrrw  x1, 0x300, x0  → x1 = old(0),  mstatus = 0    (0x300010F3)
	//   csrrwi x2, 0x300, 5   → x2 = old(0),  mstatus = 5    (0x300 2D173 → bytes
	//   below) csrrsi x3, 0x300, 2   → x3 = old(5),  mstatus = 7    (5|2) csrrci
	//   x4, 0x300, 1   → x4 = old(7),  mstatus = 6    (7&~1)
	std::vector<uint8_t> prog = {
	    0xF3, 0x10, 0x00, 0x30,  // csrrw  x1, 0x300, x0
	    0x73, 0xD1, 0x02, 0x30,  // csrrwi x2, 0x300, 5
	    0xF3, 0x61, 0x01, 0x30,  // csrrsi x3, 0x300, 2
	    0x73, 0xF2, 0x00, 0x30,  // csrrci x4, 0x300, 1
	    0x73, 0x00, 0x00, 0x00   // ecall
	};
	cpu.load(prog, 0x80000000);
	run_to_ecall(cpu);
	REQUIRE(cpu.readReg(1) == 0);  // csrrw  returns old value (0)
	REQUIRE(cpu.readReg(2) == 0);  // csrrwi returns old value (0)
	REQUIRE(cpu.readReg(3) == 5);  // csrrsi returns old value (5)
	REQUIRE(cpu.readReg(4) == 7);  // csrrci returns old value (7)
}