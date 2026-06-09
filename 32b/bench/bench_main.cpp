#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include "cpu.hpp"
#include "elf_loader.hpp"

#ifndef BENCH_ELF_PATH
#error "BENCH_ELF_PATH must be defined by CMake"
#endif

int main()
{
	ElfImage img;
	if (!load_elf(BENCH_ELF_PATH, img))
	{
		std::fprintf(stderr, "failed to load " BENCH_ELF_PATH "\n");
		return 1;
	}

	ankerl::nanobench::Bench bench;
	bench.title("rv32ui emulator").unit("run").warmup(3).minEpochIterations(20);

	uint64_t last_count = 0;

	bench.run("bench.elf throughput",
	          [&]
	          {
		          CPU cpu(img.mem_base, img.mem_size);

		          if (!cpu.load(img.data, img.mem_base))
		          {
			          throw std::runtime_error("CPU load failed");
		          }

		          cpu.run_until_halt();
		          last_count = cpu.instruction_count();
		          ankerl::nanobench::doNotOptimizeAway(cpu.instruction_count());
	          });

	auto const& results = bench.results();
	if (!results.empty())
	{
		double median_s = results[0].median(ankerl::nanobench::Result::Measure::elapsed);
		double mips     = (static_cast<double>(last_count) / median_s) / 1e6;

		std::printf("\nestimated MIPS: %.2f\n", mips);
		std::printf("instruction counter: %llu\n", static_cast<unsigned long long>(last_count));
	}

	return 0;
}