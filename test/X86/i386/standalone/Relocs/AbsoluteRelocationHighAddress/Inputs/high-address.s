.section .text,"ax",@progbits
.globl _start
_start:
  .long 0
  .reloc 0, R_386_32, target
