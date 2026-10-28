.syntax unified
.arm

.text
.balign 4

load_target:
    // U=1, imm8=1 gives A = +4.
    .inst 0xed9f0101
    .reloc load_target, R_ARM_LDC_PC_G2, target

    bx lr

    // S - P = 0x40404.
    // X = S + A - P = 0x40404 + 4 = 0x40408.
    // G2 residual = 0x8, so imm8 = 2.
    .space 0x403fc

.type target, %object
target:
    .word 42
