#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include "defs.hpp"
#include "memory.hpp"

class CPU
{
 public:
	enum class stopReason
	{
		Halt,
		Ecall,
		Ebreak
	};

	explicit CPU(uint32_t base = 0x80000000, size_t memSize = 0x10000);

	bool load(std::span<const uint8_t> data, uint32_t address);

	void run();

	void run_until_halt();

	uint32_t readReg(uint32_t address) const { return _regs[address]; }

	void writeReg(uint32_t address, uint32_t value)
	{
		if (address != 0)
		{
			_regs[address] = value;
		}
	}

	std::optional<uint32_t> peekWord(uint32_t addr) const;

	std::optional<stopReason> step();

	uint64_t instruction_count() const { return _instruction_count; }

	// Halt address for benchmarking, use run_until_halt() and when this address is written or read 
	// execute_S will catch it and set the halt flag which will then exit the program 
	static constexpr uint32_t HALT_ADDR = 0x10000000;

 private:
	std::array<uint32_t, 32>               _regs{};
	std::unordered_map<uint32_t, uint32_t> _csrs;
	uint32_t                               _pc{};
	Memory                                 _memory;
	uint64_t															 _instruction_count;
	bool 																	 _halted;		 

	uint32_t fetch();

	uint32_t read_CSR(uint32_t address) const;
	void     write_CSR(uint32_t address, uint32_t value);

	void execute(const InstructionField& field);

	void execute_R(const InstructionField& field);
	void execute_I(const InstructionField& field);
	void execute_IL(const InstructionField& field);
	void execute_S(const InstructionField& field);
	void execute_B(const InstructionField& field, uint32_t current_pc);
	void execute_U(const InstructionField& field, uint32_t current_pc);
	void execute_J(const InstructionField& field, uint32_t current_pc);
	void execute_JALR(const InstructionField& field, uint32_t current_pc);
	void execute_SYS(const InstructionField& field);
};
