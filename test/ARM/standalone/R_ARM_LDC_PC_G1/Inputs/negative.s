.syntax unified
.arm

.text
.balign 4

.type target, %object
target:
    .word 42

    // Place load_target exactly 0x1234 bytes after target.
    .space 0x1230

load_target:
    // U=1 and imm8=0 gives A = 0.
    //
    // X = S + A - P
    //   = -0x1234
    //
    // G1 residual = 0x34.
    // imm8 = 0x34 >> 2 = 0x0d.
    // Since X is negative, U must be cleared.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G1, target

    bx lr
