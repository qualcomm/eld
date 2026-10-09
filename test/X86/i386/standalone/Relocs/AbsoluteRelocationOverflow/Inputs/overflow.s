.section .text,"ax",@progbits
.globl _start
_start:
  .byte 0
  .space 0xff
  .reloc 0, R_386_8, target
target:
  .byte 0
