.syntax unified
.arm

.data
.globl target
.type target, %object
target:
    .word 42

.globl ldr_pc_g0_slot
.type ldr_pc_g0_slot, %object
ldr_pc_g0_slot:
    .reloc ldr_pc_g0_slot, R_ARM_LDR_PC_G0, target
    .word 0

.text
.globl get_target
.type get_target, %function
get_target:
    ldr r1, =ldr_pc_g0_slot
    ldr r0, [r1]
    sub r1, r1, r0
    ldr r0, [r1]
    bx lr
