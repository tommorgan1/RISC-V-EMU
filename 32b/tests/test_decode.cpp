#include <catch2/catch_test_macros.hpp>

#include "decode.hpp"

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

// Asserts an optional has a value and returns it
template <typename T>
T require_value (const std::optional<T>& opt, const char* name)
{
	INFO ("Expected " << name << " to have a value but it was nullopt");
	REQUIRE (opt.has_value ());
	return *opt;
}

#define FIELD(opt) require_value ((opt), #opt)

// -------------------------------------------------------------------------
// R-type: ADD x1, x2, x3
//
// add  rd=x1  rs1=x2  rs2=x3  funct3=0x0  funct7=0x00
//
// Encoding:
//   funct7 [31:25] = 0000000
//   rs2    [24:20] = 00011   (x3)
//   rs1    [19:15] = 00010   (x2)
//   funct3 [14:12] = 000
//   rd     [11:7]  = 00001   (x1)
//   opcode [6:0]   = 0110011 (0x33)
//
//   0000000_00011_00010_000_00001_0110011
//   = 0x00310033
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: R-type ADD x1, x2, x3", "[decode][R]")
{
	auto field = decode::decode (0x003100B3);

	REQUIRE (field.type == InstructionType::R);
	REQUIRE (field.raw == 0x003100B3);
	REQUIRE (field.opcode == 0x33);
	REQUIRE (FIELD (field.rd) == 1);
	REQUIRE (FIELD (field.rs1) == 2);
	REQUIRE (FIELD (field.rs2) == 3);
	REQUIRE (FIELD (field.funct3) == 0x0);
	REQUIRE (FIELD (field.funct7) == 0x00);
	REQUIRE_FALSE (field.imm.has_value ());
}

// -------------------------------------------------------------------------
// R-type: SUB x4, x5, x6
//
// sub  rd=x4  rs1=x5  rs2=x6  funct3=0x0  funct7=0x20
//
//   funct7 [31:25] = 0100000
//   rs2    [24:20] = 00110   (x6)
//   rs1    [19:15] = 00101   (x5)
//   funct3 [14:12] = 000
//   rd     [11:7]  = 00100   (x4)
//   opcode [6:0]   = 0110011 (0x33)
//
//   0100000_00110_00101_000_00100_0110011
//   = 0x40628233
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: R-type SUB x4, x5, x6", "[decode][R]")
{
	auto field = decode::decode (0x40628233);

	REQUIRE (field.type == InstructionType::R);
	REQUIRE (FIELD (field.rd) == 4);
	REQUIRE (FIELD (field.rs1) == 5);
	REQUIRE (FIELD (field.rs2) == 6);
	REQUIRE (FIELD (field.funct3) == 0x0);
	REQUIRE (FIELD (field.funct7) == 0x20);
}

// -------------------------------------------------------------------------
// I-type: ADDI x1, x2, -4
//
// addi  rd=x1  rs1=x2  imm=-4  funct3=0x0
//
//   imm    [31:20] = 111111111100   (-4 in 12-bit two's complement)
//   rs1    [19:15] = 00010          (x2)
//   funct3 [14:12] = 000
//   rd     [11:7]  = 00001          (x1)
//   opcode [6:0]   = 0010011        (0x13)
//
//   111111111100_00010_000_00001_0010011
//   = 0xFFC10093
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: I-type ADDI x1, x2, -4", "[decode][I]")
{
	auto field = decode::decode (0xFFC10093);

	REQUIRE (field.type == InstructionType::I);
	REQUIRE (field.opcode == 0x13);
	REQUIRE (FIELD (field.rd) == 1);
	REQUIRE (FIELD (field.rs1) == 2);
	REQUIRE (FIELD (field.funct3) == 0x0);
	REQUIRE (FIELD (field.imm) == -4);
	REQUIRE_FALSE (field.rs2.has_value ());
}

// -------------------------------------------------------------------------
// I-type: ADDI x0, x0, 0  (canonical NOP)
//
//   imm=0  rs1=x0  funct3=0  rd=x0  opcode=0x13
//   = 0x00000013
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: I-type NOP (ADDI x0, x0, 0)", "[decode][I]")
{
	auto field = decode::decode (0x00000013);

	REQUIRE (field.type == InstructionType::I);
	REQUIRE (FIELD (field.rd) == 0);
	REQUIRE (FIELD (field.rs1) == 0);
	REQUIRE (FIELD (field.imm) == 0);
}

// -------------------------------------------------------------------------
// IL-type: LW x1, -8(x2)
//
// lw  rd=x1  rs1=x2  imm=-8  funct3=0x2
//
//   imm    [31:20] = 111111111000   (-8 in 12-bit two's complement)
//   rs1    [19:15] = 00010          (x2)
//   funct3 [14:12] = 010
//   rd     [11:7]  = 00001          (x1)
//   opcode [6:0]   = 0000011        (0x03)
//
//   111111111000_00010_010_00001_0000011
//   = 0xFF812083
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: IL-type LW x1, -8(x2)", "[decode][IL]")
{
	auto field = decode::decode (0xFF812083);

	REQUIRE (field.type == InstructionType::IL);
	REQUIRE (field.opcode == 0x03);
	REQUIRE (FIELD (field.rd) == 1);
	REQUIRE (FIELD (field.rs1) == 2);
	REQUIRE (FIELD (field.funct3) == 0x2);
	REQUIRE (FIELD (field.imm) == -8);
	REQUIRE_FALSE (field.rs2.has_value ());
	REQUIRE_FALSE (field.funct7.has_value ());
}

// -------------------------------------------------------------------------
// S-type: SW x3, -8(x2)
//
// sw  rs1=x2  rs2=x3  imm=-8  funct3=0x2
//
// imm is split: imm[11:5] in [31:25], imm[4:0] in [11:7]
// -8 = 0xFF8 in 12-bit = 111111111000
//   imm[11:5] = 1111111  imm[4:0] = 11000
//
//   imm[11:5] [31:25] = 1111111
//   rs2       [24:20] = 00011   (x3)
//   rs1       [19:15] = 00010   (x2)
//   funct3    [14:12] = 010
//   imm[4:0]  [11:7]  = 11000
//   opcode    [6:0]   = 0100011 (0x23)
//
//   1111111_00011_00010_010_11000_0100011
//   = 0xFE312C23
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: S-type SW x3, -8(x2)", "[decode][S]")
{
	auto field = decode::decode (0xFE312C23);

	REQUIRE (field.type == InstructionType::S);
	REQUIRE (field.opcode == 0x23);
	REQUIRE (FIELD (field.rs1) == 2);
	REQUIRE (FIELD (field.rs2) == 3);
	REQUIRE (FIELD (field.funct3) == 0x2);
	REQUIRE (FIELD (field.imm) == -8);
	REQUIRE_FALSE (field.rd.has_value ());
	REQUIRE_FALSE (field.funct7.has_value ());
}

// -------------------------------------------------------------------------
// B-type: BEQ x1, x2, +8
//
// beq  rs1=x1  rs2=x2  imm=+8  funct3=0x0
//
// imm is split across bits: [12|10:5] in [31:25], [4:1|11] in [11:7]
// +8 = 0x008 — in B-type encoding (bit 0 always 0):
//   imm[12]   = 0
//   imm[11]   = 0
//   imm[10:5] = 000000
//   imm[4:1]  = 0100
//
//   imm[12]   [31]    = 0
//   imm[10:5] [30:25] = 000000
//   rs2       [24:20] = 00010   (x2)
//   rs1       [19:15] = 00001   (x1)
//   funct3    [14:12] = 000
//   imm[4:1]  [11:8]  = 0100
//   imm[11]   [7]     = 0
//   opcode    [6:0]   = 1100011 (0x63)
//
//   0000000_00010_00001_000_01000_1100011
//   = 0x00208463
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: B-type BEQ x1, x2, +8", "[decode][B]")
{
	auto field = decode::decode (0x00208463);

	REQUIRE (field.type == InstructionType::B);
	REQUIRE (field.opcode == 0x63);
	REQUIRE (FIELD (field.rs1) == 1);
	REQUIRE (FIELD (field.rs2) == 2);
	REQUIRE (FIELD (field.funct3) == 0x0);
	REQUIRE (FIELD (field.imm) == 8);
	REQUIRE_FALSE (field.rd.has_value ());
	REQUIRE_FALSE (field.funct7.has_value ());
}

// -------------------------------------------------------------------------
// U-type: LUI x1, 0x12345
//
// lui  rd=x1  imm=0x12345000  (upper 20 bits, lower 12 zeroed)
//
//   imm[31:12] [31:12] = 0x12345
//   rd         [11:7]  = 00001   (x1)
//   opcode     [6:0]   = 0110111 (0x37)
//
//   0x12345_00001_0110111
//   = 0x123450B7
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: U-type LUI x1, 0x12345", "[decode][U]")
{
	auto field = decode::decode (0x123450B7);

	REQUIRE (field.type == InstructionType::U);
	REQUIRE (field.opcode == 0x37);
	REQUIRE (FIELD (field.rd) == 1);
	REQUIRE (FIELD (field.imm) == static_cast<int32_t> (0x12345000));
	REQUIRE_FALSE (field.rs1.has_value ());
	REQUIRE_FALSE (field.rs2.has_value ());
	REQUIRE_FALSE (field.funct3.has_value ());
	REQUIRE_FALSE (field.funct7.has_value ());
}

// -------------------------------------------------------------------------
// J-type: JAL x1, +16
//
// jal  rd=x1  imm=+16
//
// imm is split: [20|10:1|11|19:12] across the instruction
// +16 = 0x10 in 21-bit signed:
//   imm[20]   = 0
//   imm[19:12] = 00000000
//   imm[11]   = 0
//   imm[10:1] = 0000001000
//
//   imm[20]    [31]    = 0
//   imm[10:1]  [30:21] = 0000001000
//   imm[11]    [20]    = 0
//   imm[19:12] [19:12] = 00000000
//   rd         [11:7]  = 00001      (x1)
//   opcode     [6:0]   = 1101111    (0x6F)
//
//   0_0000001000_0_00000000_00001_1101111
//   = 0x010000EF
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: J-type JAL x1, +16", "[decode][J]")
{
	auto field = decode::decode (0x010000EF);

	REQUIRE (field.type == InstructionType::J);
	REQUIRE (field.opcode == 0x6F);
	REQUIRE (FIELD (field.rd) == 1);
	REQUIRE (FIELD (field.imm) == 16);
	REQUIRE_FALSE (field.rs1.has_value ());
	REQUIRE_FALSE (field.rs2.has_value ());
	REQUIRE_FALSE (field.funct3.has_value ());
	REQUIRE_FALSE (field.funct7.has_value ());
}

// -------------------------------------------------------------------------
// JALR: JALR x1, x2, 4
//
// jalr  rd=x1  rs1=x2  imm=4  funct3=0x0
//
//   imm    [31:20] = 000000000100  (+4)
//   rs1    [19:15] = 00010         (x2)
//   funct3 [14:12] = 000
//   rd     [11:7]  = 00001         (x1)
//   opcode [6:0]   = 1100111       (0x67)
//
//   000000000100_00010_000_00001_1100111
//   = 0x00410067
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: JALR x1, x2, 4", "[decode][JALR]")
{
	auto field = decode::decode (0x004100E7);

	REQUIRE (field.type == InstructionType::JALR);
	REQUIRE (field.opcode == 0x67);
	REQUIRE (FIELD (field.rd) == 1);
	REQUIRE (FIELD (field.rs1) == 2);
	REQUIRE (FIELD (field.funct3) == 0x0);
	REQUIRE (FIELD (field.imm) == 4);
	REQUIRE_FALSE (field.rs2.has_value ());
	REQUIRE_FALSE (field.funct7.has_value ());
}

// -------------------------------------------------------------------------
// SYS: ECALL
//
//   imm=0x000  rs1=x0  funct3=0x0  rd=x0  opcode=0x73
//   = 0x00000073
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: SYS ECALL", "[decode][SYS]")
{
	auto field = decode::decode (0x00000073);

	REQUIRE (field.type == InstructionType::SYS);
	REQUIRE (field.opcode == 0x73);
	REQUIRE (FIELD (field.rd) == 0);
	REQUIRE (FIELD (field.rs1) == 0);
	REQUIRE (FIELD (field.funct3) == 0x0);
	REQUIRE (FIELD (field.imm) == 0x000);  // ECALL code, not sign-extended
}

// -------------------------------------------------------------------------
// SYS: CSRRS x1, mhartid, x0   (read mhartid into x1)
//
//   imm=0xF14  rs1=x0  funct3=0x2  rd=x1  opcode=0x73
//
//   0xF14_00000_010_00001_1110011
//   = 0xF14020F3  -- wait: rd=x1=00001, so
//   = 0xF1402073 with rd=0... let me be precise:
//
//   csr    [31:20] = 0xF14 = 111100010100
//   rs1    [19:15] = 00000 (x0)
//   funct3 [14:12] = 010
//   rd     [11:7]  = 00001 (x1)
//   opcode [6:0]   = 1110011 (0x73)
//
//   111100010100_00000_010_00001_1110011
//   = 0xF14020F3  -- note rd bits [11:7] = 00001 = 0x01, so [11:7]<<7 = 0x80
//   Let me just encode it cleanly:
//   = 0xF14020F3
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: SYS CSRRS x1, mhartid, x0", "[decode][SYS]")
{
	// csr=0xF14 (mhartid), rs1=x0, funct3=2 (CSRRS), rd=x1
	auto field = decode::decode (0xF14020F3);

	REQUIRE (field.type == InstructionType::SYS);
	REQUIRE (FIELD (field.rd) == 1);
	REQUIRE (FIELD (field.rs1) == 0);
	REQUIRE (FIELD (field.funct3) == 0x2);
	REQUIRE (FIELD (field.imm) == 0xF14);  // raw CSR address — not sign-extended
}

// -------------------------------------------------------------------------
// ILLEGAL: unknown opcode 0x02
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: ILLEGAL unknown opcode", "[decode][ILLEGAL]")
{
	uint32_t bad   = 0x00000002;  // opcode 0x02 — not a valid RISC-V opcode
	auto     field = decode::decode (bad);

	REQUIRE (field.type == InstructionType::ILLEGAL);
	REQUIRE (field.raw == bad);  // raw preserved so CPU can write it to MTVAL
}

// -------------------------------------------------------------------------
// ILLEGAL: B-type sign extend
// -------------------------------------------------------------------------

TEST_CASE ("Decoder: B-type BEQ x1, x2, -8", "[decoder][B]")
{
	// beq  rs1=x1  rs2=x2  imm=-8  funct3=0x0
	// -8 in 13-bit two's complement = 1_1111_1111_1000
	//   imm[12]   = 1
	//   imm[11]   = 1
	//   imm[10:5] = 111111
	//   imm[4:1]  = 1100
	//
	//   1_111111_00010_00001_000_1100_1_1100011
	//   = 0xFE208CE3
	auto field = decode::decode (0xFE208CE3);

	REQUIRE (field.type == InstructionType::B);
	REQUIRE (FIELD (field.rs1) == 1);
	REQUIRE (FIELD (field.rs2) == 2);
	REQUIRE (FIELD (field.funct3) == 0x0);
	REQUIRE (FIELD (field.imm) == -8);
}