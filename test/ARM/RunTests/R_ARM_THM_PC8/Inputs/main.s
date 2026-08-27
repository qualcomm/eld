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
    ldr r1, =thm_pc8_slot     @ r1 = &thm_pc8_slot
    ldrh r0, [r1]              @ r0 = imm8 (encoded offset)
    lsl r0, r0, #2             @ r0 = val = imm8 << 2
    @ Pa = (slot_addr + 4) & ~3
    add r2, r1, #4             @ r2 = slot_addr + 4
    bic r2, r2, #3             @ r2 = Pa = (slot_addr + 4) & ~3
    add r1, r2, r0             @ r1 = Pa + val = target address
    ldr r0, [r1]               @ r0 = *target = 42
    bx lr
