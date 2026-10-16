.syntax unified
.arm

.text
.balign 4

load_target:
    // U=1 and imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G2, target

    bx lr

    // S - P = 0x40402.
    // G2 residual = 0x2, which is not 4-byte aligned.
    .space 0x403fa

.type target, %object
target:
    .word 42
