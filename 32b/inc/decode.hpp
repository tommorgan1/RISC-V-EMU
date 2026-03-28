#pragma once 

#include <cstdint>
#include "defs.hpp"

namespace decode
{
  // Decode 32-bit instruction into InstructionField type 
  // Handle unknown opcode with ILLEGAL return
  // Set raw field on illegal instruction so it can be written to MTVAL 
  InstructionField decode(uint32_t instruction);
}
