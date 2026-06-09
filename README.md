## A RISC-V CPU emulator written in C++. 

---

## Features

- Full RV32I base integer ISA 
- Default Machine-mode privileged architecture, minimum implmentation needed for rv32ui-p-* compliance suite 
- Halt mechanism with run_until_halt(), there is a dedicated memory address that when read or written to will halt the program. This was specifically for benchmarking, not indicative of real hardware
- Standalone ELF loader 
- 42/42 risc-v compliance rv32ui-p-* tests passing 
- Nanobench benchmark for MIPS (million instructions per second)

---

## Instruction List

### R-type

| Instruction | Operation |
|-------------|-----------|
| ADD  | Add rs1 and rs2, write to rd |
| SUB  | Subtract rs2 from rs1, write to rd |
| SLL  | Logical left shift rs1 by rs2[4:0], write to rd |
| SLT  | Write 1 to rd if rs1 < rs2 (signed), else 0 |
| SLTU | Write 1 to rd if rs1 < rs2 (unsigned), else 0 |
| XOR  | Bitwise XOR of rs1 and rs2, write to rd |
| SRL  | Logical right shift rs1 by rs2[4:0], write to rd |
| SRA  | Arithmetic right shift rs1 by rs2[4:0], write to rd |
| OR   | Bitwise OR of rs1 and rs2, write to rd |
| AND  | Bitwise AND of rs1 and rs2, write to rd |

### I-type (ALU)

| Instruction | Operation |
|-------------|-----------|
| ADDI  | Add rs1 and sign-extended immediate, write to rd |
| SLTI  | Write 1 to rd if rs1 < immediate (signed), else 0 |
| SLTIU | Write 1 to rd if rs1 < immediate (unsigned), else 0 |
| XORI  | Bitwise XOR of rs1 and sign-extended immediate, write to rd |
| ORI   | Bitwise OR of rs1 and sign-extended immediate, write to rd |
| ANDI  | Bitwise AND of rs1 and sign-extended immediate, write to rd |
| SLLI  | Logical left shift rs1 by shamt, write to rd |
| SRLI  | Logical right shift rs1 by shamt, write to rd |
| SRAI  | Arithmetic right shift rs1 by shamt, write to rd |

### Load

| Instruction | Operation |
|-------------|-----------|
| LB  | Load sign-extended byte from rs1+imm, write to rd |
| LH  | Load sign-extended halfword from rs1+imm, write to rd |
| LW  | Load word from rs1+imm, write to rd |
| LBU | Load zero-extended byte from rs1+imm, write to rd |
| LHU | Load zero-extended halfword from rs1+imm, write to rd |

### Store

| Instruction | Operation |
|-------------|-----------|
| SB | Store low byte of rs2 to rs1+imm |
| SH | Store low halfword of rs2 to rs1+imm |
| SW | Store word of rs2 to rs1+imm |

### Branch

| Instruction | Operation |
|-------------|-----------|
| BEQ  | Branch to PC+imm if rs1 == rs2 |
| BNE  | Branch to PC+imm if rs1 != rs2 |
| BLT  | Branch to PC+imm if rs1 < rs2 (signed) |
| BGE  | Branch to PC+imm if rs1 >= rs2 (signed) |
| BLTU | Branch to PC+imm if rs1 < rs2 (unsigned) |
| BGEU | Branch to PC+imm if rs1 >= rs2 (unsigned) |

### Upper immediate

| Instruction | Operation |
|-------------|-----------|
| LUI   | Load 20-bit immediate into upper bits of rd, lower 12 bits zeroed |
| AUIPC | Add 20-bit upper immediate to PC, write to rd |

### Jump

| Instruction | Operation |
|-------------|-----------|
| JAL  | Jump to PC+imm, write return address (PC+4) to rd |
| JALR | Jump to (rs1+imm) with LSB cleared, write return address (PC+4) to rd |

### System

| Instruction | Operation |
|-------------|-----------|
| ECALL  | Environment call — jumps to MTVEC, sets MCAUSE=11 |
| EBREAK | Breakpoint — jumps to MTVEC, sets MCAUSE=3 |
| MRET   | Return from machine-mode trap, restores PC from MEPC |
| WFI    | Wait for interrupt — treated as NOP |
| CSRRW  | Write rs1 to CSR, return old value to rd |
| CSRRS  | Set bits in CSR using rs1 mask, return old value to rd |
| CSRRC  | Clear bits in CSR using rs1 mask, return old value to rd |
| CSRRWI | Write zero-extended 5-bit immediate to CSR, return old value to rd |
| CSRRSI | Set bits in CSR using 5-bit immediate mask, return old value to rd |
| CSRRCI | Clear bits in CSR using 5-bit immediate mask, return old value to rd |
| FENCE  | Memory ordering fence — NOP (single core) |

---

## Compliance 

All 42 RV32UI physical memory tests from the
[riscv-tests](https://github.com/riscv-software-src/riscv-tests) suite pass:

```
100% tests passed, 0 tests failed out of 84
```

Tests cover: `add`, `addi`, `and`, `andi`, `auipc`, `beq`, `bge`, `bgeu`,
`blt`, `bltu`, `bne`, `jal`, `jalr`, `lb`, `lbu`, `lh`, `lhu`, `lui`, `lw`,
`or`, `ori`, `sb`, `sh`, `simple`, `sll`, `slli`, `slt`, `slti`, `sltiu`,
`sltu`, `sra`, `srai`, `srl`, `srli`, `sub`, `sw`, `xor`, `xori`,
`fence_i`, `ld_st`, `ma_data`, `st_ld`.

--- 

## Benchmark 

- Measured on an MSI Prestige 16 AI Studio Intel(R) Core(TM) Ultra 7 155H 
- Pinned to isolated cores 3,4 (two cores redundant due to single threaded, in there for future expansion)
- Custom benchmarking assembly bench/bench.S
- See scripts/run_bench.sh for details 

Example bench:

|              ns/run |               run/s |    err% |     total | rv32ui emulator
|--------------------:|--------------------:|--------:|----------:|:----------------
|        2,289,736.00 |              436.73 |    0.7% |      0.56 | `bench.elf throughput`

estimated MIPS: 132.03
instruction counter: 302317

Across multiple runs MIPS stays consistent at ~130

## Analysis 

### branch analysis 

|              ns/run |               run/s |    err% |         ins/run |         cyc/run |    IPC |        bra/run |   miss% |     total | rv32ui emulator
|--------------------:|--------------------:|--------:|----------------:|----------------:|-------:|---------------:|--------:|----------:|:----------------
|        2,313,375.70 |              432.27 |    0.3% |   51,404,954.35 |    6,898,835.48 |  7.451 |   9,070,105.80 |    0.0% |      0.56 | `bench.elf throughput`

estimated MIPS: 130.68
instruction counter: 302317
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.136 MB perf.data (2295 samples) ]

- 0 branch misses, indicative of a highly predictable bench binary. Not stressing predictor enough, something to improve 
- Approx 170 host instructions per simulated instruction (51,404,954.35/302317)

### perf

- cmd: perf report --stdio --no-children -i perf.data 2>/dev/null | head -50
- --no-children reports time spent in each functions logic, not dispatched functions 


    27.96%  riscv-bench  riscv-bench           [.] decode::decode(unsigned int)
    16.95%  riscv-bench  riscv-bench           [.] CPU::execute(InstructionField const&)
    16.27%  riscv-bench  riscv-bench           [.] Memory::read32(unsigned int, unsigned int&) const
    14.99%  riscv-bench  riscv-bench           [.] CPU::run_until_halt()
    10.27%  riscv-bench  riscv-bench           [.] CPU::execute_B(InstructionField const&, unsigned int)
     7.54%  riscv-bench  riscv-bench           [.] CPU::execute_I(InstructionField const&)
     5.61%  riscv-bench  riscv-bench           [.] CPU::execute_R(InstructionField const&)
     0.09%  riscv-bench  [kernel.kallsyms]     [k] 0xffffffff8ef9e9b0
     0.09%  riscv-bench  riscv-bench           [.] void ankerl::nanobench::detail::LinuxPerformanceCounters::calibrate<ankerl::nanobench::detail::PerformanceCounters::PerformanceCounters()::{lambda()#1}>(ankerl::nanobench::detail::PerformanceCounters::PerformanceCounters()::{lambda()#1}&&) [clone .isra.0]
     0.04%  riscv-bench  libc.so.6             [.] __memmove_avx_unaligned_erms
     0.03%  riscv-bench  [kernel.kallsyms]     [k] 0xffffffff8f1567eb
     0.03%  riscv-bench  [kernel.kallsyms]     [k] 0xffffffff8ea01268
     0.03%  riscv-bench  ld-linux-x86-64.so.2  [.] _dl_relocate_object_no_relro
     0.02%  riscv-bench  [kernel.kallsyms]     [k] 0xffffffff8ee08282
     0.02%  riscv-bench  ld-linux-x86-64.so.2  [.] _dl_check_map_versions
     0.02%  riscv-bench  [kernel.kallsyms]     [k] 0xffffffff8f30cb27
     0.01%  riscv-bench  [kernel.kallsyms]     [k] 0xffffffff8f29f443
     0.01%  taskset      [kernel.kallsyms]     [k] 0xffffffff8f182c6d
     0.00%  taskset      [kernel.kallsyms]     [k] 0xffffffff8f177446
     0.00%  taskset      [kernel.kallsyms]     [k] 0xffffffff8f58f81e


- Significant time spent in decode. Upon reading, likely due to the std::optional fields in the InstructionField struct. Area for possible optimisation 

