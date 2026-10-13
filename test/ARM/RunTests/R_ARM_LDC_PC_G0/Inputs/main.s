.syntax unified
.arm
.fpu vfpv3

.text
.balign 4

.global get_target
.type get_target, %function
get_target:
load_target:
    // Initial instruction has A = -8.
    //
    // target is 12 bytes after load_target:
    //
    // X = S + A - P
    //   = (P + 12) - 8 - P
    //   = 4
    //
    // R_ARM_LDC_PC_G0 therefore encodes an offset of +4.
    //
    // At runtime the ARM architectural PC is P + 8:
    //
    // effective address = (P + 8) + 4
    //                   = P + 12
    //                   = target.
    vldr s0, [pc, #-8]
    .reloc load_target, R_ARM_LDC_PC_G0, target

    // Return the raw 32-bit value loaded into s0.
    vmov r0, s0
    bx lr

.balign 4
.type target, %object
target:
    .word 42
