.section .text,"ax",@progbits
.globl _start
_start:
  .short 0
  .reloc 0, R_386_PC16, target
