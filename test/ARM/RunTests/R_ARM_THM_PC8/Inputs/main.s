.syntax unified
.arm

@ The symbol that the R_ARM_THM_PC8 relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ thm_pc8_slot holds a R_ARM_THM_PC8 relocation to 'target', forced
@ explicitly via .reloc. R_ARM_THM_PC8 applies to a 16-bit Thumb
@ LDR Rd,[PC,#imm8*4] instruction, so the placeholder is .hword.
.globl thm_pc8_slot
.type thm_pc8_slot, %object
thm_pc8_slot:
    .reloc thm_pc8_slot, R_ARM_THM_PC8, target
    .hword 0
    .hword 0    @ padding to maintain 4-byte alignment

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
