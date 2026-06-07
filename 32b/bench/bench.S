.section .text.startup
.global _start

_start:
    # -------------------------------------------------------
    # phase 1: ALU and branch
    # sum integers 1..N, N = 100000
    # instructions per iteration: 3 (add, addi, bnez)
    # total: ~300000 instructions
    # -------------------------------------------------------
    li   t0, 0            # accumulator
    li   t1, 100000       # loop counter
1:
    add  t0, t0, t1
    addi t1, t1, -1
    bnez t1, 1b

    # -------------------------------------------------------
    # phase 2: memory stores
    # write 0xDEADBEEF to 256 consecutive words
    # base address 0x8000 (well inside 64K RAM)
    # -------------------------------------------------------
    li   t0, 0x8000
    li   t1, 256
    li   t2, 0xDEAD       # lui/addi split, see note below
    lui  t2, 0xDEADB
    addi t2, t2, 0x7EF    # approximate; assembler handles this
    li   t2, -559038737   # = 0xDEADBEEF as signed 32-bit
2:
    sw   t2, 0(t0)
    addi t0, t0, 4
    addi t1, t1, -1
    bnez t1, 2b

    # -------------------------------------------------------
    # phase 3: memory loads
    # read back those 256 words into t3, accumulate
    # -------------------------------------------------------
    li   t0, 0x8000
    li   t1, 256
    li   t3, 0
3:
    lw   t4, 0(t0)
    add  t3, t3, t4
    addi t0, t0, 4
    addi t1, t1, -1
    bnez t1, 3b

    # -------------------------------------------------------
    # halt: magic MMIO write to 0x10000000
    # store exit code 0
    # -------------------------------------------------------
    li   t0, 0x10000000
    li   t1, 0
    sw   t1, 0(t0)

    # should not reach here
.loop:
    j .loop