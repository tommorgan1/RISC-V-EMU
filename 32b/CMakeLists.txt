cmake_minimum_required(VERSION 3.16)
project(riscv-emu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch Catch2 for unit testing ─────────────────────
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# ── Core emulator library  ────────────────────────────
add_library(riscv-emu-lib STATIC
    src/memory.cpp
    src/decode.cpp
    src/cpu.cpp
    src/elf_loader.cpp
)

target_include_directories(riscv-emu-lib PUBLIC
    inc
    inc/utils
)

target_compile_features(riscv-emu-lib PUBLIC cxx_std_20) 
# Commented out section below is intentional. No explicit optimisation for testing currently 
#target_compile_options(riscv-emu-lib PRIVATE -O3 -march=native)
#set_property(TARGET riscv-emu-lib PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)


# ── Main executable ───────────────────────────────────
add_executable(riscv-emu
    src/main.cpp
)

target_link_libraries(riscv-emu PRIVATE riscv-emu-lib)

# ── Test executable ───────────────────────────────────
add_executable(riscv-emu-tests
    tests/test_memory.cpp
    tests/test_decode.cpp
    tests/test_cpu_execute.cpp
)

target_link_libraries(riscv-emu-tests PRIVATE
    riscv-emu-lib
    Catch2::Catch2WithMain   
)

include(CTest)
include(Catch)
catch_discover_tests(riscv-emu-tests)

add_subdirectory(tests/compliance)
add_subdirectory(bench)