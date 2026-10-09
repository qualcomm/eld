.section .text,"ax",@progbits
.globl _start
.type _start,@function
_start:
  .byte 0x01
  .value 0x0002
  .long 0x00000003
  .byte 0x5a
  .reloc 0, R_386_8, target
  .reloc 1, R_386_16, target
  .reloc 3, R_386_32, target
  .reloc 7, R_386_NONE, target
target:
  .byte 0
.size _start, .-_start
