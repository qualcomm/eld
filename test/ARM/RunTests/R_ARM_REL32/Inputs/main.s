.syntax unified
.arm

@ A global variable whose address we will reference PC-relatively.
.data
.globl target
.type target, %object
target:
.word 0

@ rel_slot holds a R_ARM_REL32 relocation to 'target', forced explicitly via
@ .reloc so the assembler emits the relocation rather than folding the
@ symbol difference at assembly time.
.globl rel_slot
.type rel_slot, %object
rel_slot:
    .reloc rel_slot, R_ARM_REL32, target
    .word 0

.text
.globl main
.type main, %function
main:
    @ Load the address of rel_slot into r0.
    ldr  r0, =rel_slot
    @ Load the stored PC-relative offset value (patched by the linker).
    ldr  r1, [r0]
    @ Compute: address of target = address of rel_slot + stored offset.
    add  r2, r0, r1
    @ Load the absolute address of target for comparison.
    ldr  r3, =target
    @ Return 0 if the computed address matches, 1 otherwise.
    subs r0, r2, r3
    movne r0, #1
    bx   lr
