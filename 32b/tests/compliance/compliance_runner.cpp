// tests/compliance/compliance_runner.cpp
// Loads a riscv-tests ELF, runs it on the CPU, and checks the tohost symbol.
// Pass:  mem[tohost] == 1
// Fail:  mem[tohost] == (TESTNUM << 1) | 1  (any odd value != 1)
//
// Usage: compliance_runner <test1.elf> [test2.elf ...]
// Returns 0 if all tests pass, 1 if any fail.

#include <filesystem>
#include <iostream>
#include <string>

#include "cpu.hpp"
#include "elf_loader.hpp"

static constexpr uint32_t MAX_STEPS     = 10'000'000;
static constexpr uint32_t POLL_INTERVAL = 128;

static bool run_test(const std::string& path)
{
	ElfImage    img;
	std::string name = std::filesystem::path(path).stem().string();

	if (!load_elf(path, img))
	{
		std::cout << "[ERROR ] " << name << "  (ELF parse failed)\n";
		return false;
	}

	if (img.tohost_addr == 0)
	{
		std::cout << "[ERROR ] " << name << "  (tohost symbol not found)\n";
		return false;
	}

	CPU cpu(img.mem_base, img.mem_size);
	cpu.load(img.data, img.mem_base);

	std::string crash_reason;
	for (uint32_t n = 0; n < MAX_STEPS; ++n)
	{
		if ((n & (POLL_INTERVAL - 1)) == 0)
		{
			auto val = cpu.peekWord(img.tohost_addr);
			if (val && *val != 0)
			{
				break;
			}
		}
		try
		{
			cpu.step();
		}
		catch (const std::exception& e)
		{
			crash_reason = e.what();
			break;
		}
	}

	auto tohost = cpu.peekWord(img.tohost_addr);
	bool passed = (tohost && *tohost == 1);

	if (passed)
	{
		std::cout << "[PASS  ] " << name << "\n";
	}
	else
	{
		if (!crash_reason.empty())
		{
			std::cout << "         crash: " << crash_reason << "\n";
		}
		uint32_t code    = tohost.value_or(0);
		uint32_t testnum = code >> 1;
		std::cout << "[FAIL  ] " << name << "  (tohost=0x" << std::hex << code << std::dec
		          << ", failing case " << testnum << ")\n";
	}

	return passed;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "Usage: compliance_runner <test.elf> [test2.elf ...]\n";
		return 1;
	}

	int passed = 0, failed = 0;
	for (int i = 1; i < argc; ++i)
	{
		if (run_test(argv[i]))
		{
			++passed;
		}
		else
		{
			++failed;
		}
	}

	std::cout << "\n"
	          << (passed + failed) << " tests: " << passed << " passed, " << failed << " failed\n";

	return (failed == 0) ? 0 : 1;
}