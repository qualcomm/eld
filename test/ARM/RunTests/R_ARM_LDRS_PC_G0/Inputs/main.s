.syntax unified
.arm

.text
.balign 4

.global get_target
.type get_target, %function
get_target:
    // LDRH literal with U=0, imm8=8.
    // This gives the relocation implicit addend A = -8,
    // compensating for the ARM architectural PC bias.
    .inst 0xe15f00b8
    .reloc get_target, R_ARM_LDRS_PC_G0, target
    bx lr

.balign 4
.type target, %object
target:
    .hword 42
