.section .text,"ax",@progbits
.globl _start
_start:
  .byte 0
  .space 0x7f
  .reloc 0, R_386_PC8, target
target:
  .byte 0
