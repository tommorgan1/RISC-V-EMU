cmake_minimum_required(VERSION 3.16)
project(riscv-emu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add library target
add_library(riscv-emu-lib
    src/cpu.cpp
    src/memory.cpp
    src/main.cpp
)

# Include directories
target_include_directories(riscv-emu-lib PUBLIC
    inc
    inc/utils
)

# Ensure C++20 features
target_compile_features(riscv-emu-lib PUBLIC cxx_std_20)