.syntax unified
.arm

@ The symbol that the R_ARM_SBREL32 relocation will reference.
@ Placed in .text so it shares the same PT_LOAD segment as
@ sbrel_slot, making B(S) the same for both.
.text
.globl target
.type target, %object
target:
    .word 0

@ sbrel_slot holds a R_ARM_SBREL32 relocation to 'target', forced explicitly
@ via .reloc. The linker patches sbrel_slot with (addr(target) - B(S))
@ where B(S) is the base virtual address of the PT_LOAD segment containing
@ 'target'. Both sbrel_slot and target are in .text so they share the same
@ segment; B(S) equals __executable_start.
.globl sbrel_slot
.type sbrel_slot, %object
sbrel_slot:
    .reloc sbrel_slot, R_ARM_SBREL32, target
    .word 0

.globl main
.type main, %function
main:
    @ Load the stored segment-relative offset (patched by the linker).
    ldr  r0, =sbrel_slot
    ldr  r1, [r0]
    @ Compute: addr(target) = B(S) + sbrel_slot value.
    @ B(S) is the vaddr of the PT_LOAD segment, i.e. __executable_start.
    ldr  r2, =__executable_start
    add  r2, r2, r1
    @ Load the absolute address of target for comparison.
    ldr  r3, =target
    @ Return 0 if computed address matches, 1 otherwise.
    subs r0, r2, r3
    movne r0, #1
    bx   lr
