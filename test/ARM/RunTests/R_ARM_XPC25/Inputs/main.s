.syntax unified
.arm

@ The symbol that the R_ARM_XPC25 relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ xpc25_slot holds a R_ARM_XPC25 relocation to 'target', forced
@ explicitly via .reloc.
.globl xpc25_slot
.type xpc25_slot, %object
xpc25_slot:
    .reloc xpc25_slot, R_ARM_XPC25, target
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
