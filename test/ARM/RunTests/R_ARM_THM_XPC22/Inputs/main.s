.syntax unified
.arm

@ The symbol that the R_ARM_THM_XPC22 relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ thm_xpc22_slot holds a R_ARM_THM_XPC22 relocation to 'target', forced
@ explicitly via .reloc.
.globl thm_xpc22_slot
.type thm_xpc22_slot, %object
thm_xpc22_slot:
    .reloc thm_xpc22_slot, R_ARM_THM_XPC22, target
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
