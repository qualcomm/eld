.section .text,"ax",@progbits
.globl _start
.type _start,@function
_start:
  .byte 0x01
  .value 0x0002
  .long 0x00000003
  .byte 0
  .reloc 0, R_386_PC8, target
  .reloc 1, R_386_PC16, target
  .reloc 3, R_386_PC32, target
target:
  .byte 0
.size _start, .-_start
