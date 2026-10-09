.syntax unified
.arm

.text
.balign 4

load_target:
    // U=1 and imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G1, target

    bx lr

    // X = target - load_target = 0x40400.
    // G1 residual = 0x400, which is outside [0, 0x3ff].
    .space 0x403f8

.type target, %object
target:
    .word 42
