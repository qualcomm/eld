.syntax unified
.arm

.text
.balign 4

.global get_target
.type get_target, %function
get_target:
add_base:
    // A = -8 compensates for the ARM PC bias.
    sub r1, pc, #8
    .reloc add_base, R_ARM_ALU_PC_G0_NC, target

load_target:
    // This instruction is 4 bytes after add_base, so A = -4 makes
    // both relocations operate on the same X value.
    ldrh r0, [r1, #-4]
    .reloc load_target, R_ARM_LDRS_PC_G1, target

    bx lr

    // target - add_base = 0x123c.
    // With A = -8:
    // X = 0x123c - 8 = 0x1234.
    .space 0x1230

.balign 4
.type target, %object
target:
    .hword 42
