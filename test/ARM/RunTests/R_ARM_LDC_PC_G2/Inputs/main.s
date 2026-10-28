.syntax unified
.arm
.fpu vfpv3

.text
.balign 4

.global get_target
.type get_target, %function
get_target:
add_base:
    // A = -8 compensates for the ARM PC bias.
    sub r1, pc, #8
    .reloc add_base, R_ARM_ALU_PC_G0_NC, target

add_middle:
    // This instruction is 4 bytes after add_base, so A = -4 makes
    // this relocation operate on the same X value.
    sub r1, r1, #4
    .reloc add_middle, R_ARM_ALU_PC_G1_NC, target

load_target:
    // This instruction is 8 bytes after add_base. A = 0 makes
    // this relocation operate on the same X value.
    vldr s0, [r1, #0]
    .reloc load_target, R_ARM_LDC_PC_G2, target

    vmov r0, s0
    bx lr

    // target - add_base = 0x4040c.
    // With A = -8 at add_base:
    // X = 0x4040c - 8 = 0x40404.
    //
    // R_ARM_LDC_PC_G2 selects the G2 residual:
    // G2 residual = 0x4
    // imm8 = 0x4 >> 2 = 1
    .space 0x403f8

.balign 4
.type target, %object
target:
    .word 42
