.syntax unified
.arm

@ The symbol that the R_ARM_ABS8 relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ abs8_slot holds a R_ARM_ABS8 relocation to 'target', forced explicitly
@ via .reloc.
.globl abs8_slot
.type abs8_slot, %object
abs8_slot:
    .reloc abs8_slot, R_ARM_ABS8, target
    .byte 0
    .byte 0     @ padding
    .byte 0     @ padding
    .byte 0     @ padding to keep alignment

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
