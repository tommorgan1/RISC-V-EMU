# tests/compliance/CMakeLists.txt
# Subdirectory — no cmake_minimum_required here, that lives in 32b/CMakeLists.txt

set(RISCV_TESTS_DIR "${CMAKE_SOURCE_DIR}/../external/riscv-tests")

# ---- Sanity check ---------------------------------------------------------
if(NOT EXISTS "${RISCV_TESTS_DIR}/isa/rv32ui")
    message(WARNING
        "riscv-tests submodule not found at ${RISCV_TESTS_DIR}.\n"
        "Run: git submodule update --init --recursive")
    return()
endif()

# ---- Build the runner -----------------------------------------------------
add_executable(compliance_runner
    ${CMAKE_CURRENT_SOURCE_DIR}/compliance_runner.cpp
)
target_include_directories(compliance_runner PRIVATE "${CMAKE_SOURCE_DIR}/inc")
target_link_libraries(compliance_runner PRIVATE riscv-emu-lib)
target_compile_features(compliance_runner PRIVATE cxx_std_20)

# ---- Find RISC-V toolchain ------------------------------------------------
find_program(RISCV_GCC NAMES
    riscv64-unknown-elf-gcc
    riscv32-unknown-elf-gcc
    riscv64-linux-gnu-gcc
)

if(NOT RISCV_GCC)
    message(WARNING
        "No RISC-V cross-compiler found — compliance ELFs will not be built.\n"
        "Install with:  sudo apt install gcc-riscv64-unknown-elf  (in WSL)")
    return()
endif()
message(STATUS "RISC-V compiler: ${RISCV_GCC}")

# rv32i_zicsr needed for GCC >= 12 (CSR instructions are a separate extension).
# If your toolchain errors on this, change to just: rv32i
set(RISCV_ARCH rv32i_zicsr_zifencei)
set(RISCV_ABI  ilp32)

set(RISCV_FLAGS
    -march=${RISCV_ARCH}
    -mabi=${RISCV_ABI}
    -static
    -mcmodel=medany
    -fvisibility=hidden
    -nostdlib
    -nostartfiles
    -I${RISCV_TESTS_DIR}/isa/macros/scalar
    -I${RISCV_TESTS_DIR}/env/p
    -T${RISCV_TESTS_DIR}/env/p/link.ld
)

# ---- Compile each rv32ui test to an ELF -----------------------------------
file(GLOB RV32UI_SOURCES "${RISCV_TESTS_DIR}/isa/rv32ui/*.S")

set(ELF_DIR "${CMAKE_CURRENT_BINARY_DIR}/elfs")
file(MAKE_DIRECTORY "${ELF_DIR}")

set(ALL_ELFS "")
foreach(SRC IN LISTS RV32UI_SOURCES)
    get_filename_component(STEM "${SRC}" NAME_WE)
    set(ELF "${ELF_DIR}/rv32ui-p-${STEM}")
    add_custom_command(
        OUTPUT  "${ELF}"
        COMMAND "${RISCV_GCC}" ${RISCV_FLAGS} "${SRC}" -o "${ELF}"
        DEPENDS "${SRC}"
        COMMENT "  [riscv-tests] ${STEM}"
        VERBATIM
    )
    list(APPEND ALL_ELFS "${ELF}")
endforeach()

# Build all ELFs with:  cmake --build build --target rv32ui_elfs
add_custom_target(rv32ui_elfs DEPENDS ${ALL_ELFS})
add_dependencies(compliance_runner rv32ui_elfs)

# ---- Register each ELF as a CTest case ------------------------------------
foreach(ELF IN LISTS ALL_ELFS)
    get_filename_component(TEST_NAME "${ELF}" NAME)
    add_test(
        NAME    "compliance/${TEST_NAME}"
        COMMAND compliance_runner "${ELF}"
    )
    set_tests_properties("compliance/${TEST_NAME}" PROPERTIES LABELS "compliance")
endforeach()