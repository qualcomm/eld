.syntax unified
.arm

.text
.balign 4

.type target, %object
target:
    .word 42

    .space 0x40400

load_target:
    // U=0, imm8=1 gives A = -4.
    //
    // S - P = -0x40404.
    // X = S + A - P = -0x40404 - 4 = -0x40408.
    // G2 residual = 0x8, so imm8 = 2 and U remains clear.
    .inst 0xed1f0101
    .reloc load_target, R_ARM_LDC_PC_G2, target

    bx lr
