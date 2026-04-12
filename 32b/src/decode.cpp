#include "decode.hpp"

#include "bits.hpp"

namespace
{
InstructionField decode_R (uint32_t instruction)
{
	return {.type   = InstructionType::R,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = (instruction >> 7) & 0x1F,
	        .rs1    = (instruction >> 15) & 0x1F,
	        .rs2    = (instruction >> 20) & 0x1F,
	        .funct3 = (instruction >> 12) & 0x7,
	        .funct7 = (instruction >> 25) & 0x7F,
	        .imm    = std::nullopt};
}

InstructionField decode_I (uint32_t instruction)
{
	return {.type   = InstructionType::I,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = (instruction >> 7) & 0x1F,
	        .rs1    = (instruction >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instruction >> 12) & 0x7,
	        .funct7 = (instruction >> 25) & 0x7F,
	        .imm    = bits::sign_extend ((instruction >> 20) & 0xFFF, 12)};
}

InstructionField decode_IL (uint32_t instruction)
{
	return {.type   = InstructionType::IL,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = (instruction >> 7) & 0x1F,
	        .rs1    = (instruction >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instruction >> 12) & 0x7,
	        .funct7 = std::nullopt,
	        .imm    = bits::sign_extend ((instruction >> 20) & 0xFFF, 12)};
}

InstructionField decode_JALR (uint32_t instruction)
{
	return {.type   = InstructionType::JALR,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = (instruction >> 7) & 0x1F,
	        .rs1    = (instruction >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instruction >> 12) & 0x7,
	        .funct7 = std::nullopt,
	        .imm    = bits::sign_extend ((instruction >> 20) & 0xFFF, 12)};
}

InstructionField decode_S (uint32_t instruction)
{
	int32_t imm =
	    bits::sign_extend (((instruction >> 25) & 0x7F) << 5 | ((instruction >> 7) & 0x1F), 12);

	return {.type   = InstructionType::S,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = std::nullopt,
	        .rs1    = (instruction >> 15) & 0x1F,
	        .rs2    = (instruction >> 20) & 0x1F,
	        .funct3 = (instruction >> 12) & 0x7,
	        .funct7 = std::nullopt,
	        .imm    = imm};
}

InstructionField decode_B (uint32_t instruction)
{
	int32_t imm =
	    bits::sign_extend (((instruction >> 31) & 0x1) << 12 | ((instruction >> 7) & 0x1) << 11 |
	                           ((instruction >> 25) & 0x3F) << 5 | ((instruction >> 8) & 0xF) << 1,
	                       13);

	return {.type   = InstructionType::B,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = std::nullopt,
	        .rs1    = (instruction >> 15) & 0x1F,
	        .rs2    = (instruction >> 20) & 0x1F,
	        .funct3 = (instruction >> 12) & 0x7,
	        .funct7 = std::nullopt,
	        .imm    = imm};
}

InstructionField decode_U (uint32_t instruction)
{
	return {.type   = InstructionType::U,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = (instruction >> 7) & 0x1F,
	        .rs1    = std::nullopt,
	        .rs2    = std::nullopt,
	        .funct3 = std::nullopt,
	        .funct7 = std::nullopt,
	        .imm    = static_cast<int32_t> (instruction & 0xFFFFF000)};
}

InstructionField decode_J (uint32_t instruction)
{
	int32_t imm =
	    bits::sign_extend (((instruction >> 31) & 0x1) << 20 | ((instruction >> 12) & 0xFF) << 12 |
	                           ((instruction >> 20) & 0x1) << 11 | ((instruction >> 21) & 0x3FF) << 1,
	                       21);

	return {.type   = InstructionType::J,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = (instruction >> 7) & 0x1F,
	        .rs1    = std::nullopt,
	        .rs2    = std::nullopt,
	        .funct3 = std::nullopt,
	        .funct7 = std::nullopt,
	        .imm    = imm};
}

InstructionField decode_SYS (uint32_t instruction)
{
	return {.type   = InstructionType::SYS,
	        .raw    = instruction,
	        .opcode = instruction & 0x7F,
	        .rd     = (instruction >> 7) & 0x1F,
	        .rs1    = (instruction >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instruction >> 12) & 0x7,
	        .funct7 = std::nullopt,
	        .imm    = static_cast<int32_t> ((instruction >> 20) & 0xFFF)};
}
}  // namespace

namespace decode
{
InstructionField decode (uint32_t instruction)
{
	uint32_t opcode = instruction & 0x7F;

	switch (opcode)
	{
		case 0x33:
			return decode_R (instruction);
		case 0x13:
			return decode_I (instruction);
		case 0x03:
			return decode_IL (instruction);
		case 0x67:
			return decode_JALR (instruction);
		case 0x23:
			return decode_S (instruction);
		case 0x63:
			return decode_B (instruction);
		case 0x37:
		case 0x17:
			return decode_U (instruction);
		case 0x6F:
			return decode_J (instruction);
		case 0x73:
			return decode_SYS (instruction);
		case 0x0F:
			return {.type = InstructionType::FENCE};

		default:
			return {.type = InstructionType::ILLEGAL, .raw = instruction, .opcode = opcode};
	}
}
}  // namespace decode