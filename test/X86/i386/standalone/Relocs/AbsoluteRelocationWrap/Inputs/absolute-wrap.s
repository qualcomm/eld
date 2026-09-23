.section .text,"ax",@progbits
.globl _start
_start:
  .byte 0x10
  .value 0x0010
  .reloc 0, R_386_8, target
  .reloc 1, R_386_16, target
