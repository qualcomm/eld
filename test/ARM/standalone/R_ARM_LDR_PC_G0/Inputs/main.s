.syntax unified
.arm

.globl target
.type target, %object
target:
    .word 0

.globl ldr_pc_g0_slot
.type ldr_pc_g0_slot, %object
ldr_pc_g0_slot:
    .reloc ldr_pc_g0_slot, R_ARM_LDR_PC_G0, target
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
