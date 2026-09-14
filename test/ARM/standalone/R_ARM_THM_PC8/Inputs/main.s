.syntax unified
.arm

.text
.globl thm_pc8_slot
.type thm_pc8_slot, %object
thm_pc8_slot:
    .reloc thm_pc8_slot, R_ARM_THM_PC8, target
    .hword 0
    .hword 0

.globl target
.type target, %object
target:
    .word 0

.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
