.syntax unified
.arm

.section .text.1,"ax"
.balign 4

load_target:
    // U=1 and imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G2, target

.section .text.2,"ax"
.balign 4

.type target, %object
target:
    .word 42
