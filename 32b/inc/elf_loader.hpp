#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ElfImage
{
	std::vector<uint8_t> data;
	uint32_t             mem_base    = 0;
	uint32_t             mem_size    = 0;
	uint32_t             entry_point = 0;
	uint32_t             tohost_addr = 0;  // 0 if not present
};

// Loads an ELF32 LE RISC-V binary from disk.
// Returns false if the file cannot be opened or is not a valid ELF32 RISC-V image.
bool load_elf(const std::string& path, ElfImage& img);