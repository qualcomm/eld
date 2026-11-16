.section .text.claimed,"ax",@progbits
.p2align 2
.globl _start
_start:
{ jump claimed_far }
.section .text.other,"ax",@progbits
.p2align 2
.globl other_branch
other_branch:
{ jump other_far }
.section .text.near,"ax",@progbits
.p2align 2
.globl near_branch
near_branch:
{ jump claimed_near }
.section .text.near_target,"ax",@progbits
.p2align 2
.globl claimed_near
.hidden claimed_near
claimed_near:
{ jumpr r31 }
.section .text.far,"ax",@progbits
.p2align 2
.globl claimed_far
.hidden claimed_far
claimed_far:
{ jumpr r31 }
.globl other_far
.hidden other_far
other_far:
{ jumpr r31 }
