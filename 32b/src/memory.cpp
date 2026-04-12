#include "memory.hpp"

// TODO add template function for constructing words from bytes to utils for use in read16, read32

Memory::Memory (uint32_t base, size_t size) : _base (base), _data (size, 0)
{
}

bool Memory::translate (uint32_t address, size_t access_size, size_t& index) const
{
	if (address < _base)
	{
		return false;
	}

	size_t offset = address - _base;

	if (offset + access_size > _data.size ())
	{
		return false;
	}

	index = offset;
	return true;
}

bool Memory::load (std::span<const uint8_t> data, uint32_t address)
{
	size_t index;

	if (!translate (address, data.size (), index))
	{
		return false;
	}

	std::copy (data.begin (), data.end (), _data.begin () + index);

	return true;
}

memFault Memory::read8 (uint32_t address, uint8_t& out) const
{
	size_t index;

	if (!translate (address, 1, index))
	{
		return memFault::outOfBounds;
	}

	out = _data[index];

	return memFault::none;
}

memFault Memory::read16 (uint32_t address, uint16_t& out) const
{
	size_t index;
	if (!translate (address, 2, index)) return memFault::outOfBounds;
	out = static_cast<uint16_t> (_data[index]) | static_cast<uint16_t> (_data[index + 1]) << 8;
	return memFault::none;
}

memFault Memory::read32 (uint32_t address, uint32_t& out) const
{
	size_t index;
	if (!translate (address, 4, index))
	{
		return memFault::outOfBounds;
	}

	out = static_cast<uint32_t> (_data[index]) | static_cast<uint32_t> (_data[index + 1]) << 8 |
	      static_cast<uint32_t> (_data[index + 2]) << 16 |
	      static_cast<uint32_t> (_data[index + 3]) << 24;

	return memFault::none;
}

memFault Memory::write8 (uint32_t address, uint8_t value)
{
	size_t index;
	if (!translate (address, 1, index))
	{
		return memFault::outOfBounds;
	}

	_data[index] = value;

	return memFault::none;
}

memFault Memory::write16 (uint32_t address, uint16_t value)
{
	size_t index;

	if (!translate (address, 2, index))
	{
		return memFault::outOfBounds;
	}

	_data[index]     = value & 0xFF;
	_data[index + 1] = (value >> 8) & 0xFF;

	return memFault::none;
}

memFault Memory::write32 (uint32_t address, uint32_t value)
{
	size_t index;

	if (!translate (address, 4, index))
	{
		return memFault::outOfBounds;
	}

	_data[index]     = value & 0xFF;
	_data[index + 1] = (value >> 8) & 0xFF;
	_data[index + 2] = (value >> 16) & 0xFF;
	_data[index + 3] = (value >> 24) & 0xFF;

	return memFault::none;
}
