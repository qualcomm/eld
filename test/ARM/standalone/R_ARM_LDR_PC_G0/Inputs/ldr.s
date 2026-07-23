.syntax unified
.arm

/* target data object */
.globl target
.type target, %object
target:
    .word 42

/* data slot with R_ARM_LDR_PC_G0 relocation applied */
.globl ldr_pc_g0_slot
.type ldr_pc_g0_slot, %object
ldr_pc_g0_slot:
    .reloc ldr_pc_g0_slot, R_ARM_LDR_PC_G0, target
    .word 0
