.syntax unified
.arm

.text
.balign 4

.type target, %object
target:
    .word 42

    // Place load_target exactly 0x40404 bytes after target.
    .space 0x40400

load_target:
    // U=1 and imm8=0 gives A = 0.
    //
    // X = S + A - P = -0x40404.
    // G2 residual = 0x4.
    // imm8 = 0x4 >> 2 = 0x1.
    // Since X is negative, U is cleared.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G2, target

    bx lr
