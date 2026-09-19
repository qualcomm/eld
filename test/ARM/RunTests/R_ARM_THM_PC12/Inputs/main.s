.syntax unified
.thumb

.text
.balign 4
.global get_target
.thumb_func
.type get_target, %function
get_target:
    // LDR literal T2 with U=0, imm12=4.
    // This gives the relocation implicit addend A = -4.
    .inst.w 0xf85f0004
    .reloc get_target, R_ARM_THM_PC12, target
    bx lr

.balign 4
.globl target
.type target, %object
target:
    .word 42
