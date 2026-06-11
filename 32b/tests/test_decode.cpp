#include <catch2/catch_test_macros.hpp>

#include "decode.hpp"

TEST_CASE("Decoder: R-type ADD x1, x2, x3", "[decode][R]")
{
	auto field = decode::decode(0x003100B3);

	REQUIRE(field.type == InstructionType::R);
	REQUIRE(field.raw == 0x003100B3);
	REQUIRE(field.opcode == 0x33);
	REQUIRE(field.rd == 1);
	REQUIRE(field.rs1 == 2);
	REQUIRE(field.rs2 == 3);
	REQUIRE(field.funct3 == 0x0);
	REQUIRE(field.funct7 == 0x00);
}

TEST_CASE("Decoder: R-type SUB x4, x5, x6", "[decode][R]")
{
	auto field = decode::decode(0x40628233);

	REQUIRE(field.type == InstructionType::R);
	REQUIRE(field.rd == 4);
	REQUIRE(field.rs1 == 5);
	REQUIRE(field.rs2 == 6);
	REQUIRE(field.funct3 == 0x0);
	REQUIRE(field.funct7 == 0x20);
}

TEST_CASE("Decoder: I-type ADDI x1, x2, -4", "[decode][I]")
{
	auto field = decode::decode(0xFFC10093);

	REQUIRE(field.type == InstructionType::I);
	REQUIRE(field.opcode == 0x13);
	REQUIRE(field.rd == 1);
	REQUIRE(field.rs1 == 2);
	REQUIRE(field.funct3 == 0x0);
	REQUIRE(field.imm == -4);
}

TEST_CASE("Decoder: I-type NOP (ADDI x0, x0, 0)", "[decode][I]")
{
	auto field = decode::decode(0x00000013);

	REQUIRE(field.type == InstructionType::I);
	REQUIRE(field.rd == 0);
	REQUIRE(field.rs1 == 0);
	REQUIRE(field.imm == 0);
}

TEST_CASE("Decoder: IL-type LW x1, -8(x2)", "[decode][IL]")
{
	auto field = decode::decode(0xFF812083);

	REQUIRE(field.type == InstructionType::IL);
	REQUIRE(field.opcode == 0x03);
	REQUIRE(field.rd == 1);
	REQUIRE(field.rs1 == 2);
	REQUIRE(field.funct3 == 0x2);
	REQUIRE(field.imm == -8);
}

TEST_CASE("Decoder: S-type SW x3, -8(x2)", "[decode][S]")
{
	auto field = decode::decode(0xFE312C23);

	REQUIRE(field.type == InstructionType::S);
	REQUIRE(field.opcode == 0x23);
	REQUIRE(field.rs1 == 2);
	REQUIRE(field.rs2 == 3);
	REQUIRE(field.funct3 == 0x2);
	REQUIRE(field.imm == -8);
}

TEST_CASE("Decoder: B-type BEQ x1, x2, +8", "[decode][B]")
{
	auto field = decode::decode(0x00208463);

	REQUIRE(field.type == InstructionType::B);
	REQUIRE(field.opcode == 0x63);
	REQUIRE(field.rs1 == 1);
	REQUIRE(field.rs2 == 2);
	REQUIRE(field.funct3 == 0x0);
	REQUIRE(field.imm == 8);
}

TEST_CASE("Decoder: U-type LUI x1, 0x12345", "[decode][U]")
{
	auto field = decode::decode(0x123450B7);

	REQUIRE(field.type == InstructionType::U);
	REQUIRE(field.opcode == 0x37);
	REQUIRE(field.rd == 1);
	REQUIRE(field.imm == static_cast<int32_t>(0x12345000));
}

TEST_CASE("Decoder: J-type JAL x1, +16", "[decode][J]")
{
	auto field = decode::decode(0x010000EF);

	REQUIRE(field.type == InstructionType::J);
	REQUIRE(field.opcode == 0x6F);
	REQUIRE(field.rd == 1);
	REQUIRE(field.imm == 16);
}

TEST_CASE("Decoder: JALR x1, x2, 4", "[decode][JALR]")
{
	auto field = decode::decode(0x004100E7);

	REQUIRE(field.type == InstructionType::JALR);
	REQUIRE(field.opcode == 0x67);
	REQUIRE(field.rd == 1);
	REQUIRE(field.rs1 == 2);
	REQUIRE(field.funct3 == 0x0);
	REQUIRE(field.imm == 4);
}

TEST_CASE("Decoder: SYS ECALL", "[decode][SYS]")
{
	auto field = decode::decode(0x00000073);

	REQUIRE(field.type == InstructionType::SYS);
	REQUIRE(field.opcode == 0x73);
	REQUIRE(field.rd == 0);
	REQUIRE(field.rs1 == 0);
	REQUIRE(field.funct3 == 0x0);
	REQUIRE(field.imm == 0x000);
}

TEST_CASE("Decoder: SYS CSRRS x1, mhartid, x0", "[decode][SYS]")
{
	auto field = decode::decode(0xF14020F3);

	REQUIRE(field.type == InstructionType::SYS);
	REQUIRE(field.rd == 1);
	REQUIRE(field.rs1 == 0);
	REQUIRE(field.funct3 == 0x2);
	REQUIRE(field.imm == 0xF14);
}

TEST_CASE("Decoder: ILLEGAL unknown opcode", "[decode][ILLEGAL]")
{
	uint32_t bad   = 0x00000002;
	auto     field = decode::decode(bad);

	REQUIRE(field.type == InstructionType::ILLEGAL);
	REQUIRE(field.raw == bad);
}

TEST_CASE("Decoder: B-type BEQ x1, x2, -8", "[decoder][B]")
{
	auto field = decode::decode(0xFE208CE3);

	REQUIRE(field.type == InstructionType::B);
	REQUIRE(field.rs1 == 1);
	REQUIRE(field.rs2 == 2);
	REQUIRE(field.funct3 == 0x0);
	REQUIRE(field.imm == -8);
}