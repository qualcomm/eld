.syntax unified
.arm

@ The symbol that the R_ARM_LDR_PC_G0 relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ ldr_pc_g0_slot holds a R_ARM_LDR_PC_G0 relocation to 'target', forced
@ explicitly via .reloc.
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
