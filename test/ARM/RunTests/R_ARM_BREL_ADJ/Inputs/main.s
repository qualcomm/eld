.syntax unified
.arm

@ The symbol that the R_ARM_BREL_ADJ relocation will reference.
.data
.globl target
.type target, %object
target:
    .word 0

@ brel_adj_slot holds a R_ARM_BREL_ADJ relocation to 'target', forced
@ explicitly via .reloc.
.globl brel_adj_slot
.type brel_adj_slot, %object
brel_adj_slot:
    .reloc brel_adj_slot, R_ARM_BREL_ADJ, target
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
