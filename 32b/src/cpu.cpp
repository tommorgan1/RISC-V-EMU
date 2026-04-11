#include "cpu.hpp"
#include "decode.hpp"

#include <sstream>
#include <iostream>
#include <iomanip>

static std::string hex32(uint32_t v) 
{
    std::ostringstream oss;
    oss << "" << std::hex << std::uppercase << std::setw(8)
        << std::setfill('0') << v;
    return oss.str();
}

CPU::CPU(uint32_t base, size_t memSize):
         _memory(base, memSize),
         _pc(base)
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

uint32_t CPU::readCSR(uint32_t address) const 
{
  auto it = _csrs.find(address);
  if (it == _csrs.end())
  {
    return 0;
  }
  return it->second;
}

void CPU::writeCSR(uint32_t address, uint32_t value)
{
  _csrs[address] = value;
}

std::optional<CPU::stopReason> CPU::step()
{
  uint32_t instruction = fetch();
  InstructionField field = decode::decode(instruction);
  execute(field);
  return std::nullopt;
}

void CPU::run()
{
  while (true)
  {
    step();
  }
}

std::optional<uint32_t> CPU::peekWord(uint32_t address) const
{
  uint32_t value{};
  if (_memory.read32(address, value) != memFault::none) return std::nullopt;
  return value;
}

void CPU::execute(const InstructionField& field)
{
  uint32_t currentPC = _pc - 4;

  switch(field.type)
  {
    case InstructionType::R:    execute_R   (field);                   break;
    case InstructionType::I:    execute_I   (field);                   break;
    case InstructionType::IL:   execute_IL  (field);                   break;
    case InstructionType::S:    execute_S   (field);                   break;
    case InstructionType::B:    execute_B   (field, currentPC);        break;
    case InstructionType::U:    execute_U   (field, currentPC);        break;
    case InstructionType::J:    execute_J   (field, currentPC);        break;
    case InstructionType::JALR: execute_JALR(field, currentPC);        break;
    case InstructionType::SYS:  execute_SYS (field);                   break;
    case InstructionType::FENCE:                                       break;

    case InstructionType::ILLEGAL:
      throw std::runtime_error("Illegal instruction: " + hex32(field.raw) +
                                " at PC: "              + hex32(currentPC));
  }  
}

void CPU::execute_R(const InstructionField& field)
{
  uint32_t rs1 = readReg(field.rs1.value());
  uint32_t rs2 = readReg(field.rs2.value());
  uint32_t result = 0;

  switch ((field.funct3.value() << 8) | field.funct7.value())
  {
    case (0x0 << 8) | 0x00: result = rs1 + rs2;                                                         break;
    case (0x0 << 8) | 0x20: result = rs1 - rs2;                                                         break;
    case (0x1 << 8) | 0x00: result = rs1 << (rs2 & 0x1F);                                               break;
    case (0x2 << 8) | 0x00: result = (static_cast<int32_t>(rs1) < static_cast<int32_t>(rs2)) ? 1 : 0;   break;
    case (0x3 << 8) | 0x00: result = (rs1 < rs2) ? 1 : 0;                                               break;
    case (0x4 << 8) | 0x00: result = rs1 ^ rs2;                                                         break;
    case (0x5 << 8) | 0x00: result = rs1 >> (rs2 & 0x1F);                                               break;
    case (0x5 << 8) | 0x20: result = static_cast<uint32_t>(static_cast<int32_t>(rs1) >> (rs2 & 0x1F));  break;
    case (0x6 << 8) | 0x00: result = rs1 | rs2;                                                         break;
    case (0x7 << 8) | 0x00: result = rs1 & rs2;                                                         break;

    default:
      throw std::runtime_error("Unknown R-type funct3/funct7 at PC: " + hex32(_pc - 4)); break;
  }

  writeReg(field.rd.value(), result);
}

void CPU::execute_I(const InstructionField& field)
{
  uint32_t rs1 = readReg(field.rs1.value());
  int32_t imm = field.imm.value();
  uint32_t shamt = imm & 0x1F;
  uint32_t result = 0;

  switch (field.funct3.value())
  {
    case 0x0: result = static_cast<uint32_t>(static_cast<int32_t>(rs1) + imm);  break;
    case 0x2: result = (static_cast<int32_t>(rs1) < imm) ? 1 : 0;               break;
    case 0x3: result = (rs1 < static_cast<uint32_t>(imm)) ? 1 : 0;              break;
    case 0x4: result = rs1 ^ static_cast<uint32_t>(imm);                        break;
    case 0x6: result = rs1 | static_cast<uint32_t>(imm);                        break;
    case 0x7: result = rs1 & static_cast<uint32_t>(imm);                        break;
    case 0x1: result = rs1 << shamt;                                            break;

    case 0x5:
    switch (field.funct7.value())
    {
      case 0x00: result = rs1 >> shamt;                                               break;
      case 0x20: result = static_cast<uint32_t>(static_cast<int32_t>(rs1) >> shamt);  break;
      default:
        throw std::runtime_error("Unknown funct7 for I-type shift at PC: " + hex32(_pc - 4)); break;
    }
    break;

    default:
      throw std::runtime_error("Unknown funct3 for I-type at PC: " + hex32(_pc - 4));
  }

  writeReg(field.rd.value(), result);
}

void CPU::execute_IL(const InstructionField& field)
{
  uint32_t addr   = static_cast<uint32_t>(static_cast<int32_t>(readReg(field.rs1.value())) + field.imm.value());
  uint32_t result = 0;

  switch (field.funct3.value())
  {
    case 0x0: {  // LB — sign-extended byte
      uint8_t val{};
      if (_memory.read8(addr, val) != memFault::none)
        throw std::runtime_error("LB memory fault at " + hex32(addr));
      result = static_cast<uint32_t>(static_cast<int8_t>(val));
      break;
    }
    case 0x1: {  // LH — sign-extended halfword
      uint16_t val{};
      if (_memory.read16(addr, val) != memFault::none)
        throw std::runtime_error("LH memory fault at " + hex32(addr));
      result = static_cast<uint32_t>(static_cast<int16_t>(val));
      break;
    }
    case 0x2: {  // LW
      if (_memory.read32(addr, result) != memFault::none)
        throw std::runtime_error("LW memory fault at " + hex32(addr));
      break;
    }
    case 0x4: {  // LBU — zero-extended byte
      uint8_t val{};
      if (_memory.read8(addr, val) != memFault::none)
        throw std::runtime_error("LBU memory fault at " + hex32(addr));
      result = static_cast<uint32_t>(val);
      break;
    }
    case 0x5: {  // LHU — zero-extended halfword
      uint16_t val{};
      if (_memory.read16(addr, val) != memFault::none)
        throw std::runtime_error("LHU memory fault at " + hex32(addr));
      result = static_cast<uint32_t>(val);
      break;
    }
    default:
      throw std::runtime_error("Unknown funct3 for load at PC: " + hex32(_pc - 4));
  }

  writeReg(field.rd.value(), result);
}

void CPU::execute_S(const InstructionField& field)
{
  uint32_t addr = static_cast<uint32_t>(static_cast<int32_t>(readReg(field.rs1.value())) + field.imm.value());
  uint32_t src  = readReg(field.rs2.value());
  memFault  f   = memFault::none;

  switch (field.funct3.value())
  {
    case 0x0: f = _memory.write8 (addr, static_cast<uint8_t> (src));  break;  // SB
    case 0x1: f = _memory.write16(addr, static_cast<uint16_t>(src));  break;  // SH
    case 0x2: f = _memory.write32(addr, src);                         break;  // SW
    default:
      throw std::runtime_error("Unknown funct3 for S-type at PC: " + hex32(_pc - 4));
  }

  if (f != memFault::none)
    throw std::runtime_error("Store memory fault at " + hex32(addr));
}

void CPU::execute_B(const InstructionField& field, uint32_t current_pc)
{
  uint32_t rs1   = readReg(field.rs1.value());
  uint32_t rs2   = readReg(field.rs2.value());
  int32_t  imm   = field.imm.value();
  bool     taken = false;

  switch (field.funct3.value())
  {
    case 0x0: taken = (rs1 == rs2);                                                          break;  // BEQ
    case 0x1: taken = (rs1 != rs2);                                                          break;  // BNE
    case 0x4: taken = (static_cast<int32_t>(rs1) <  static_cast<int32_t>(rs2));             break;  // BLT
    case 0x5: taken = (static_cast<int32_t>(rs1) >= static_cast<int32_t>(rs2));             break;  // BGE
    case 0x6: taken = (rs1 < rs2);                                                           break;  // BLTU
    case 0x7: taken = (rs1 >= rs2);                                                          break;  // BGEU
    default:
      throw std::runtime_error("Unknown funct3 for B-type at PC: " + hex32(_pc - 4));
  }

  if (taken)
    _pc = static_cast<uint32_t>(static_cast<int32_t>(current_pc) + imm) ;
}

void CPU::execute_U(const InstructionField& field, uint32_t current_pc)
{
  uint32_t imm = static_cast<uint32_t>(field.imm.value());

  switch (field.opcode)
  {
    case 0x37: writeReg(field.rd.value(), imm);                 break;  // LUI
    case 0x17: writeReg(field.rd.value(),  current_pc + imm) ;  break;  // AUIPC
    default:
      throw std::runtime_error("Unknown opcode for U-type at PC: " + hex32(_pc - 4));
  }
}

void CPU::execute_J(const InstructionField& field, uint32_t current_pc)
{
  writeReg(field.rd.value(), (current_pc + 4));
  _pc = static_cast<uint32_t>(static_cast<int32_t>(current_pc) + field.imm.value()) ;
}

void CPU::execute_JALR(const InstructionField& field, uint32_t current_pc)
{
  uint32_t target  = static_cast<uint32_t>(static_cast<int32_t>(readReg(field.rs1.value())) + field.imm.value());
  writeReg(field.rd.value(), (current_pc + 4));
  _pc              = (target & ~1u) ;
}

void CPU::execute_SYS(const InstructionField& field)
{
  uint32_t csr  = static_cast<uint32_t>(field.imm.value());
  uint32_t rs1v = readReg(field.rs1.value());
  uint32_t zimm = field.rs1.value();  // zero-extended 5-bit uimm for *I variants

  switch (field.funct3.value())
  {
    case 0x0:  // ECALL / EBREAK / MRET / WFI
      if (csr == 0x000) {  // ECALL
        // If a trap handler is installed, take the machine-mode trap.
        // Otherwise throw so unit-test harnesses can catch it.
        uint32_t mtvec = readCSR(MTVEC);
        if (mtvec != 0) {
          writeCSR(MEPC,   _pc - 4);  // PC of the ecall instruction
          writeCSR(MCAUSE, 11);       // environment call from M-mode
          writeCSR(MTVAL,  0);
          _pc = mtvec & ~3u;          // direct-mode: base address
        } else {
          throw std::runtime_error("ECALL");
        }
      } else if (csr == 0x001) {  // EBREAK
        uint32_t mtvec = readCSR(MTVEC);
        if (mtvec != 0) {
          writeCSR(MEPC,   _pc - 4);
          writeCSR(MCAUSE, 3);        // breakpoint
          writeCSR(MTVAL,  0);
          _pc = mtvec & ~3u;
        } else {
          throw std::runtime_error("EBREAK");
        }
      } else if (csr == 0x302) { _pc = readCSR(0x341); }  // MRET
      else if (csr == 0x105) { /* WFI — NOP for single-core */ }
      else throw std::runtime_error("Unknown SYS imm at PC: " + hex32(_pc - 4));
      break;

    case 0x1: {  // CSRRW
      uint32_t old = readCSR(csr);
      writeCSR(csr, rs1v);
      writeReg(field.rd.value(), old);
      break;
    }
    case 0x2: {  // CSRRS
      uint32_t old = readCSR(csr);
      if (rs1v != 0) writeCSR(csr, old | rs1v);
      writeReg(field.rd.value(), old);
      break;
    }
    case 0x3: {  // CSRRC
      uint32_t old = readCSR(csr);
      if (rs1v != 0) writeCSR(csr, old & ~rs1v);
      writeReg(field.rd.value(), old);
      break;
    }
    case 0x5: {  // CSRRWI
      uint32_t old = readCSR(csr);
      writeCSR(csr, zimm);
      writeReg(field.rd.value(), old);
      break;
    }
    case 0x6: {  // CSRRSI
      uint32_t old = readCSR(csr);
      if (zimm != 0) writeCSR(csr, old | zimm);
      writeReg(field.rd.value(), old);
      break;
    }
    case 0x7: {  // CSRRCI
      uint32_t old = readCSR(csr);
      if (zimm != 0) writeCSR(csr, old & ~zimm);
      writeReg(field.rd.value(), old);
      break;
    }
    default:
      throw std::runtime_error("Unknown funct3 for SYS at PC: " + hex32(_pc - 4));
  }
}