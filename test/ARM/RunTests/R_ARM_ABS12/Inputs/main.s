.syntax unified
.arm

@ The symbol that the R_ARM_ABS12 relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ abs12_slot holds a R_ARM_ABS12 relocation to 'target', forced explicitly
@ via .reloc.
.globl abs12_slot
.type abs12_slot, %object
abs12_slot:
    .reloc abs12_slot, R_ARM_ABS12, target
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
