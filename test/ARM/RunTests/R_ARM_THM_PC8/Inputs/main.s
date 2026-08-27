.syntax unified
.arm

.data
.globl thm_pc8_slot
.type thm_pc8_slot, %object
thm_pc8_slot:
    .reloc thm_pc8_slot, R_ARM_THM_PC8, target
    .hword 0
    .hword 0

.globl target
.type target, %object
target:
    .word 42

.text
.globl get_target
.type get_target, %function
get_target:
    ldr r1, =thm_pc8_slot
    ldrh r0, [r1]
    lsl r0, r0, #2
    add r1, r1, r0
    ldr r0, [r1]
    bx lr
