#include <cstdint>

namespace bits
{
  int32_t sign_extend(uint32_t value, uint32_t bits)
  {
    uint32_t sign_bit = 1 << (bits - 1);
    return (value ^ sign_bit) - sign_bit;
  }

  uint32_t get_bits(uint32_t value, uint32_t start, uint32_t length)
  {
    uint32_t mask = (1 << length) - 1;
    return (value >> start) & mask;
  }
}