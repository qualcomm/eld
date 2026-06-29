.syntax unified
.arm

@ The symbol whose lower 16 bits of address will be stored by R_ARM_ABS16.
.data
.globl target
.type target, %object
target:
    .word 0

@ abs16_slot holds a R_ARM_ABS16 relocation to 'target', forced explicitly
@ via .reloc. The linker patches this halfword with the low 16 bits of
@ target's absolute address: abs16_slot = addr(target) & 0xffff.
.globl abs16_slot
.type abs16_slot, %object
abs16_slot:
    .reloc abs16_slot, R_ARM_ABS16, target
    .hword 0
    .hword 0    @ padding to keep alignment

.text
.globl main
.type main, %function
main:
    @ Load the halfword stored at abs16_slot (low 16 bits of target's address).
    ldr  r0, =abs16_slot
    ldrh r1, [r0]
    @ Compute the expected value: addr(target) & 0xffff.
    ldr  r2, =target
    uxth r2, r2
    @ Return 0 if the patched value matches, 1 otherwise.
    subs r0, r1, r2
    movne r0, #1
    bx   lr
