.syntax unified
.arm
.fpu vfpv3

.text
.balign 4

.global get_target
.type get_target, %function
get_target:
add_base:
    // A = -8 compensates for the ARM architectural PC bias.
    sub r1, pc, #8
    .reloc add_base, R_ARM_ALU_PC_G0_NC, target

load_target:
    // load_target is 4 bytes after add_base.
    // A = -4 makes both relocations operate on the same X value.
    vldr s0, [r1, #-4]
    .reloc load_target, R_ARM_LDC_PC_G1, target

    vmov r0, s0
    bx lr

    // target - add_base = 0x123c.
    //
    // For add_base:
    // X = 0x123c - 8 = 0x1234.
    //
    // For load_target:
    // S - P = 0x1238, and A = -4:
    // X = 0x1238 - 4 = 0x1234.
    //
    // R_ARM_LDC_PC_G1 therefore selects residual 0x34,
    // encoded as imm8 = 0x34 >> 2 = 0x0d.
    .space 0x122c

.balign 4
.type target, %object
target:
    .word 42
