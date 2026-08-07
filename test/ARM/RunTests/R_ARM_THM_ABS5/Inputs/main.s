.syntax unified
.arm

@ The symbol that the R_ARM_THM_ABS5 relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ thm_abs5_slot holds a R_ARM_THM_ABS5 relocation to 'target', forced
@ explicitly via .reloc.
.globl thm_abs5_slot
.type thm_abs5_slot, %object
thm_abs5_slot:
    .reloc thm_abs5_slot, R_ARM_THM_ABS5, target
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
