.section .text.startup
.global _start

_start:
    # -------------------------------------------------------
    # phase 1: ALU and branch
    # sum integers 1..100000
    # 3 insns/iter x 100000 = 300000 instructions
    # -------------------------------------------------------
    li    t0, 0
    li    t1, 100000
1:
    add   t0, t0, t1
    addi  t1, t1, -1
    bnez  t1, 1b

    # -------------------------------------------------------
    # phase 2: store 256 words to RAM at 0x8000
    # 4 insns/iter x 256 = 1024 instructions
    # -------------------------------------------------------
    li    t0, 0x8000
    li    t1, 256
    li    t2, -559038737        # 0xDEADBEEF as signed 32-bit
2:
    sw    t2, 0(t0)
    addi  t0, t0, 4
    addi  t1, t1, -1
    bnez  t1, 2b

    # -------------------------------------------------------
    # phase 3: load those 256 words back, accumulate
    # 5 insns/iter x 256 = 1280 instructions
    # -------------------------------------------------------
    li    t0, 0x8000
    li    t1, 256
    li    t3, 0
3:
    lw    t4, 0(t0)
    add   t3, t3, t4
    addi  t0, t0, 4
    addi  t1, t1, -1
    bnez  t1, 3b

    # -------------------------------------------------------
    # halt: magic MMIO write to 0x10000000
    # -------------------------------------------------------
    li    t0, 0x10000000
    li    t1, 0
    sw    t1, 0(t0)

.loop:
    j     .loop