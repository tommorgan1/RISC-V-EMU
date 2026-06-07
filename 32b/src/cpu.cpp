#include "cpu.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "decode.hpp"

static std::string hex32(uint32_t v)
{
	std::ostringstream oss;
	oss << "" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << v;
	return oss.str();
}

CPU::CPU(uint32_t base, size_t memSize)
  : _memory(base, memSize)
  , _pc(base)
{
}

bool CPU::load(std::span<const uint8_t> data, uint32_t address)
{
	return _memory.load(data, address);
}

uint32_t CPU::fetch()
{
	uint32_t instruction{};
	memFault fault = _memory.read32(_pc, instruction);

	if (fault == memFault::misaligned)
	{
		throw std::runtime_error("Fetch misaligned at PC: " + hex32(_pc));
	}
	else if (fault == memFault::outOfBounds)
	{
		throw std::runtime_error("Fetch out of bounds at PC: " + hex32(_pc));
	}

	_pc += 4;
	return instruction;
}

uint32_t CPU::read_CSR(uint32_t address) const
{
	auto it = _csrs.find(address);
	if (it == _csrs.end())
	{
		return 0;
	}
	return it->second;
}

void CPU::write_CSR(uint32_t address, uint32_t value)
{
	_csrs[address] = value;
}

std::optional<CPU::stopReason> CPU::step()
{
	uint32_t         instruction = fetch();
	InstructionField field       = decode::decode(instruction);
	execute(field);
	++_instruction_count;
	return std::nullopt;
}

void CPU::run()
{
	while (true)
	{
		step();
	}
}

void CPU::run_until_halt()
{
	while (!_halted)
	{
		step();
	}
}

std::optional<uint32_t> CPU::peekWord(uint32_t address) const
{
	uint32_t value{};
	if (_memory.read32(address, value) != memFault::none)
	{
		return std::nullopt;
	}
	return value;
}

void CPU::execute(const InstructionField& field)
{
	uint32_t currentPC = _pc - 4;

	switch (field.type)
	{
		case InstructionType::R:
			execute_R(field);
			break;
		case InstructionType::I:
			execute_I(field);
			break;
		case InstructionType::IL:
			execute_IL(field);
			break;
		case InstructionType::S:
			execute_S(field);
			break;
		case InstructionType::B:
			execute_B(field, currentPC);
			break;
		case InstructionType::U:
			execute_U(field, currentPC);
			break;
		case InstructionType::J:
			execute_J(field, currentPC);
			break;
		case InstructionType::JALR:
			execute_JALR(field, currentPC);
			break;
		case InstructionType::SYS:
			execute_SYS(field);
			break;
		case InstructionType::FENCE:
			break;

		case InstructionType::ILLEGAL:
			throw std::runtime_error("Illegal instruction: " + hex32(field.raw) +
			                         " at PC: " + hex32(currentPC));
	}
}

void CPU::execute_R(const InstructionField& field)
{
	uint32_t rs1    = readReg(field.rs1.value());
	uint32_t rs2    = readReg(field.rs2.value());
	uint32_t result = 0;

	switch ((field.funct3.value() << 8) | field.funct7.value())
	{
		case (0x0 << 8) | 0x00:
			result = rs1 + rs2;  // ADD
			break;
		case (0x0 << 8) | 0x20:
			result = rs1 - rs2;  // SUB
			break;
		case (0x1 << 8) | 0x00:
			result = rs1 << (rs2 & 0x1F);  // SLL
			break;
		case (0x2 << 8) | 0x00:
			result = (static_cast<int32_t>(rs1) < static_cast<int32_t>(rs2)) ? 1 : 0;  // SLT
			break;
		case (0x3 << 8) | 0x00:
			result = (rs1 < rs2) ? 1 : 0;  // SLTU
			break;
		case (0x4 << 8) | 0x00:
			result = rs1 ^ rs2;  // XOR
			break;
		case (0x5 << 8) | 0x00:
			result = rs1 >> (rs2 & 0x1F);  // SRL
			break;
		case (0x5 << 8) | 0x20:
			result = static_cast<uint32_t>(static_cast<int32_t>(rs1) >> (rs2 & 0x1F));  // SRA
			break;
		case (0x6 << 8) | 0x00:
			result = rs1 | rs2;  // OR
			break;
		case (0x7 << 8) | 0x00:
			result = rs1 & rs2;  // AND
			break;

		default:
			throw std::runtime_error("Unknown R-type funct3/funct7 at PC: " + hex32(_pc - 4));
			break;
	}

	writeReg(field.rd.value(), result);
}

void CPU::execute_I(const InstructionField& field)
{
	uint32_t rs1    = readReg(field.rs1.value());
	int32_t  imm    = field.imm.value();
	uint32_t shamt  = imm & 0x1F;
	uint32_t result = 0;

	switch (field.funct3.value())
	{
		case 0x0:
			result = static_cast<uint32_t>(static_cast<int32_t>(rs1) + imm);  // ADDI
			break;
		case 0x2:
			result = (static_cast<int32_t>(rs1) < imm) ? 1 : 0;  // SLTI
			break;
		case 0x3:
			result = (rs1 < static_cast<uint32_t>(imm)) ? 1 : 0;  // SLTIU
			break;
		case 0x4:
			result = rs1 ^ static_cast<uint32_t>(imm);  // XORI
			break;
		case 0x6:
			result = rs1 | static_cast<uint32_t>(imm);  // ORI
			break;
		case 0x7:
			result = rs1 & static_cast<uint32_t>(imm);  // ANDI
			break;
		case 0x1:
			result = rs1 << shamt;  // SLLI
			break;

		case 0x5:
			switch (field.funct7.value())
			{
				case 0x00:
					result = rs1 >> shamt;  // SRLI
					break;
				case 0x20:
					result = static_cast<uint32_t>(static_cast<int32_t>(rs1) >> shamt);  // SRAI
					break;
				default:
					throw std::runtime_error("Unknown funct7 for I-type shift at PC: " + hex32(_pc - 4));
					break;
			}
			break;

		default:
			throw std::runtime_error("Unknown funct3 for I-type at PC: " + hex32(_pc - 4));
	}

	writeReg(field.rd.value(), result);
}

void CPU::execute_IL(const InstructionField& field)
{
	uint32_t addr =
	    static_cast<uint32_t>(static_cast<int32_t>(readReg(field.rs1.value())) + field.imm.value());
	uint32_t result = 0;

	switch (field.funct3.value())
	{
		case 0x0:
		{
			uint8_t val{};
			if (_memory.read8(addr, val) != memFault::none)
			{
				throw std::runtime_error("LB memory fault at " + hex32(addr));
			}
			result = static_cast<uint32_t>(static_cast<int8_t>(val));  // LB — sign-extended byte
			break;
		}
		case 0x1:
		{
			uint16_t val{};
			if (_memory.read16(addr, val) != memFault::none)
			{
				throw std::runtime_error("LH memory fault at " + hex32(addr));
			}
			result = static_cast<uint32_t>(static_cast<int16_t>(val));  // LH — sign-extended halfword
			break;
		}
		case 0x2:
		{
			if (_memory.read32(addr, result) != memFault::none)
			{
				throw std::runtime_error("LW memory fault at " + hex32(addr));
			}
			break;  // LW
		}
		case 0x4:
		{
			uint8_t val{};
			if (_memory.read8(addr, val) != memFault::none)
			{
				throw std::runtime_error("LBU memory fault at " + hex32(addr));
			}
			result = static_cast<uint32_t>(val);  // LBU — zero-extended byte
			break;
		}
		case 0x5:
		{
			uint16_t val{};
			if (_memory.read16(addr, val) != memFault::none)
			{
				throw std::runtime_error("LHU memory fault at " + hex32(addr));
			}
			result = static_cast<uint32_t>(val);  // LHU — zero-extended halfword
			break;
		}
		default:
			throw std::runtime_error("Unknown funct3 for load at PC: " + hex32(_pc - 4));
	}

	writeReg(field.rd.value(), result);
}

void CPU::execute_S(const InstructionField& field)
{
	uint32_t addr =
	    static_cast<uint32_t>(static_cast<int32_t>(readReg(field.rs1.value())) + field.imm.value());
	uint32_t src = readReg(field.rs2.value());

	if (addr == HALT_ADDR)
	{
		_halted = true;
		return;
	}

	memFault f   = memFault::none;

	switch (field.funct3.value())
	{
		case 0x0:
			f = _memory.write8(addr, static_cast<uint8_t>(src));  // SB
			break;
		case 0x1:
			f = _memory.write16(addr, static_cast<uint16_t>(src));  // SH
			break;
		case 0x2:
			f = _memory.write32(addr, src);  // SW
			break;
		default:
			throw std::runtime_error("Unknown funct3 for S-type at PC: " + hex32(_pc - 4));
	}

	if (f != memFault::none)
	{
		throw std::runtime_error("Store memory fault at " + hex32(addr));
	}
}

void CPU::execute_B(const InstructionField& field, uint32_t current_pc)
{
	uint32_t rs1   = readReg(field.rs1.value());
	uint32_t rs2   = readReg(field.rs2.value());
	int32_t  imm   = field.imm.value();
	bool     taken = false;

	switch (field.funct3.value())
	{
		case 0x0:
			taken = (rs1 == rs2);  // BEQ
			break;
		case 0x1:
			taken = (rs1 != rs2);  // BNE
			break;
		case 0x4:
			taken = (static_cast<int32_t>(rs1) < static_cast<int32_t>(rs2));  // BLT
			break;
		case 0x5:
			taken = (static_cast<int32_t>(rs1) >= static_cast<int32_t>(rs2));  // BGE
			break;
		case 0x6:
			taken = (rs1 < rs2);  // BLTU
			break;
		case 0x7:
			taken = (rs1 >= rs2);  // BGEU
			break;
		default:
			throw std::runtime_error("Unknown funct3 for B-type at PC: " + hex32(_pc - 4));
	}

	if (taken)
	{
		_pc = static_cast<uint32_t>(static_cast<int32_t>(current_pc) + imm);
	}
}

void CPU::execute_U(const InstructionField& field, uint32_t current_pc)
{
	uint32_t imm = static_cast<uint32_t>(field.imm.value());

	switch (field.opcode)
	{
		case 0x37:
			writeReg(field.rd.value(), imm);  // LUI
			break;
		case 0x17:
			writeReg(field.rd.value(), current_pc + imm);  // AUIPC
			break;
		default:
			throw std::runtime_error("Unknown opcode for U-type at PC: " + hex32(_pc - 4));
	}
}

void CPU::execute_J(const InstructionField& field, uint32_t current_pc)
{
	writeReg(field.rd.value(), (current_pc + 4));  // JAL — save return address
	_pc = static_cast<uint32_t>(static_cast<int32_t>(current_pc) +
	                            field.imm.value());  // JAL — jump to target
}

void CPU::execute_JALR(const InstructionField& field, uint32_t current_pc)
{
	uint32_t target =
	    static_cast<uint32_t>(static_cast<int32_t>(readReg(field.rs1.value())) + field.imm.value());
	writeReg(field.rd.value(), (current_pc + 4));  // JALR — save return address
	_pc = (target & ~1u);                          // JALR — jump to target, clear LSB
}

void CPU::execute_SYS(const InstructionField& field)
{
	uint32_t csr  = static_cast<uint32_t>(field.imm.value());
	uint32_t rs1v = readReg(field.rs1.value());
	uint32_t zimm = field.rs1.value();  // zero-extended 5-bit uimm for *I variants

	switch (field.funct3.value())
	{
		case 0x0:  // ECALL / EBREAK / MRET / WFI
			if (csr == 0x000)
			{
				uint32_t mtvec = read_CSR(MTVEC);
				if (mtvec != 0)
				{
					write_CSR(MEPC, _pc - 4);  // ECALL — save PC of ecall instruction
					write_CSR(MCAUSE, 11);     // ECALL — environment call from M-mode
					write_CSR(MTVAL, 0);
					_pc = mtvec & ~3u;  // ECALL — jump to trap handler (direct mode)
				}
				else
				{
					throw std::runtime_error("ECALL");
				}
			}
			else if (csr == 0x001)
			{
				uint32_t mtvec = read_CSR(MTVEC);
				if (mtvec != 0)
				{
					write_CSR(MEPC, _pc - 4);  // EBREAK — save PC of ebreak instruction
					write_CSR(MCAUSE, 3);      // EBREAK — breakpoint cause
					write_CSR(MTVAL, 0);
					_pc = mtvec & ~3u;  // EBREAK — jump to trap handler (direct mode)
				}
				else
				{
					throw std::runtime_error("EBREAK");
				}
			}
			else if (csr == 0x302)
			{
				_pc = read_CSR(0x341);  // MRET — return from machine-mode trap, restore PC from MEPC
			}
			else if (csr == 0x105)
			{
				/* WFI — NOP for single-core */
			}
			else
			{
				throw std::runtime_error("Unknown SYS imm at PC: " + hex32(_pc - 4));
			}
			break;

		case 0x1:
		{
			uint32_t old = read_CSR(csr);
			write_CSR(csr, rs1v);              // CSRRW — write rs1 to CSR
			writeReg(field.rd.value(), old);  // CSRRW — return old CSR value
			break;
		}
		case 0x2:
		{
			uint32_t old = read_CSR(csr);
			if (rs1v != 0)
			{
				write_CSR(csr, old | rs1v);  // CSRRS — set bits in CSR
			}
			writeReg(field.rd.value(), old);  // CSRRS — return old CSR value
			break;
		}
		case 0x3:
		{
			uint32_t old = read_CSR(csr);
			if (rs1v != 0)
			{
				write_CSR(csr, old & ~rs1v);  // CSRRC — clear bits in CSR
			}
			writeReg(field.rd.value(), old);  // CSRRC — return old CSR value
			break;
		}
		case 0x5:
		{
			uint32_t old = read_CSR(csr);
			write_CSR(csr, zimm);              // CSRRWI — write uimm to CSR
			writeReg(field.rd.value(), old);  // CSRRWI — return old CSR value
			break;
		}
		case 0x6:
		{
			uint32_t old = read_CSR(csr);
			if (zimm != 0)
			{
				write_CSR(csr, old | zimm);  // CSRRSI — set bits in CSR using uimm
			}
			writeReg(field.rd.value(), old);  // CSRRSI — return old CSR value
			break;
		}
		case 0x7:
		{
			uint32_t old = read_CSR(csr);
			if (zimm != 0)
			{
				write_CSR(csr, old & ~zimm);  // CSRRCI — clear bits in CSR using uimm
			}
			writeReg(field.rd.value(), old);  // CSRRCI — return old CSR value
			break;
		}
		default:
			throw std::runtime_error("Unknown funct3 for SYS at PC: " + hex32(_pc - 4));
	}
}