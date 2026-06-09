# ── Cross-compiled benchmark ELF (RV32I bare-metal) ──────────────────────────

set(RISCV_CC riscv64-unknown-elf-gcc)

set(BENCH_ELF ${CMAKE_CURRENT_BINARY_DIR}/bench.elf)

add_custom_command(
    OUTPUT  ${BENCH_ELF}
    COMMAND ${RISCV_CC}
            -march=rv32i
            -mabi=ilp32
            -nostdlib
            -nostartfiles
            -T ${CMAKE_CURRENT_SOURCE_DIR}/link.ld
            -o ${BENCH_ELF}
            ${CMAKE_CURRENT_SOURCE_DIR}/bench.S
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bench.S
            ${CMAKE_CURRENT_SOURCE_DIR}/link.ld
    COMMENT "Cross-compiling bench.elf for RV32I"
)

add_custom_target(bench_elf ALL DEPENDS ${BENCH_ELF})

# ── Fetch nanobench ───────────────────────────────────────────────────────────

include(FetchContent)
FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG        v4.3.11
)
FetchContent_MakeAvailable(nanobench)

# ── Benchmark harness (host binary) ──────────────────────────────────────────

add_executable(riscv-bench
    bench_main.cpp
)

target_link_libraries(riscv-bench PRIVATE
    riscv-emu-lib
    nanobench
)

# bench_elf must be built before the harness runs, but it's not a link dep
add_dependencies(riscv-bench bench_elf)

# pass the ELF path in as a compile definition so bench_main.cpp can find it
target_compile_definitions(riscv-bench PRIVATE
    BENCH_ELF_PATH="${BENCH_ELF}"
)