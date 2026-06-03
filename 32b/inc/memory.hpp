#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

enum class memFault
{
	none,
	misaligned,
	outOfBounds
};

class Memory
{
 public:
	// Memory
	// Base: physical base address of the memory region
	// Size: size in bytes
	explicit Memory(uint32_t baseAddress = 0x80000000, size_t size = 0x10000);

	// Load a binary image into memory at given address
	// Return false if image does not fit
	bool load(std::span<const uint8_t> data, uint32_t address);

	// Reads
	// Value returned with the out parameter, fault through return value
	memFault read8(uint32_t address, uint8_t &out) const;
	memFault read16(uint32_t address, uint16_t &out) const;
	memFault read32(uint32_t address, uint32_t &out) const;

	// Writes
	memFault write8(uint32_t address, uint8_t value);
	memFault write16(uint32_t address, uint16_t value);
	memFault write32(uint32_t address, uint32_t value);

 private:
	std::vector<uint8_t> _data;
	uint32_t             _base;

	// Translates physical address of vector index
	// Returns false if out of bounds
	bool translate(uint32_t address, size_t access_size, size_t &index) const;
};