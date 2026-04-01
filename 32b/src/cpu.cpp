#include "cpu.hpp"
#include "decode.hpp"

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
    throw std::runtime_error("Fetch misaligned at PC: " + std::to_string(_pc));
  }
  if (fault == memFault::outOfBounds)
  {
    throw std::runtime_error("Fetch out of bounds at PC: " + std::to_string(_pc));
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
        case InstructionType::SYS:  execute_SYS (field); 

        case InstructionType::ILLEGAL:
          throw std::runtime_error("Illegal instruction: 0x" + std::to_string(field.raw) +
                                   " at PC: 0x"              + std::to_string(currentPC));
  }
  _regs[0] = 0;
}

void CPU::run()
{
  while (true)
  {
    uint32_t instruction = fetch();
    InstructionField field = decode::decode(instruction);
    execute(field);
  }
}

void CPU::execute_S   (const InstructionField&)              { throw std::runtime_error("execute_S not implemented");    }
void CPU::execute_B   (const InstructionField&, uint32_t)    { throw std::runtime_error("execute_B not implemented");    }
void CPU::execute_U   (const InstructionField&, uint32_t)    { throw std::runtime_error("execute_U not implemented");    }
void CPU::execute_J   (const InstructionField&, uint32_t)    { throw std::runtime_error("execute_J not implemented");    }
void CPU::execute_JALR(const InstructionField&, uint32_t)    { throw std::runtime_error("execute_JALR not implemented"); }
void CPU::execute_SYS (const InstructionField&)              { throw std::runtime_error("execute_SYS not implemented");  }

void CPU::execute_R(const InstructionField& field)
{
  uint32_t rs1 = _regs[*field.rs1];
  uint32_t rs2 = _regs[*field.rs2];
  uint32_t result = 0;

  switch ((*field.funct3 << 8) | *field.funct7)
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
      throw std::runtime_error("Unknown R-type funct3/funct7 at PC: 0x" + 
                               std::to_string(_pc - 4)); break;
  }

  _regs[*field.rd] = result & MASK32;
}

void CPU::execute_I(const InstructionField& field)
{
  uint32_t rs1 = _regs[*field.rs1];
  int32_t imm = *field.imm;
  uint32_t shamt = imm & 0x1F;
  uint32_t result = 0;

  switch (*field.funct3)
  {
    case 0x0: result = static_cast<uint32_t>(static_cast<int32_t>(rs1) + imm);  break;
    case 0x2: result = (static_cast<int32_t>(rs1) < imm) ? 1 : 0;               break;
    case 0x3: result = (rs1 < static_cast<uint32_t>(imm)) ? 1 : 0;              break;
    case 0x4: result = rs1 ^ static_cast<uint32_t>(imm);                        break;
    case 0x6: result = rs1 | static_cast<uint32_t>(imm);                        break;
    case 0x7: result = rs1 & static_cast<uint32_t>(imm);                        break;
    case 0x1: result = rs1 << shamt;                                            break;

    case 0x5:
    switch (*field.funct7)
    {
      case 0x00: result = rs1 >> shamt;                                               break;
      case 0x20: result = static_cast<uint32_t>(static_cast<int32_t>(rs1) >> shamt);  break;
      default:
        throw std::runtime_error("Unknown funct7 for I-type shift at PC: 0x" +
                                 std::to_string(_pc - 4)); break;
    }
    break;
  }

  _regs[*field.rd] = result & MASK32;
}

void CPU::execute_IL(const InstructionField& field)
{

}

