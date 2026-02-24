.section .text.ov1, "ax"
.p2align 0
.globl ov1_sym
ov1_sym:
  .space 0x10, 0

.section .text.ov2, "ax"
.p2align 0
.globl ov2_sym
ov2_sym:
  .space 0x20, 0

.section .text.after, "ax"
.p2align 0
.globl _start
_start:
  .space 0x4, 0

