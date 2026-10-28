.syntax unified
.arm

.text
.balign 4

    // Keep target at 0x2 while load_target remains instruction-aligned.
    .space 2

.type target, %object
target:
    .word 42

    // load_target = 0x40404.
    // S - P = 0x2 - 0x40404 = -0x40402.
    .space 0x403fe

load_target:
    // U=1 and imm8=0 gives A = 0.
    // G2 residual = 0x2, which is not 4-byte aligned.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G2, target
