.section .text,"ax",@progbits
.globl _start
.type _start,@function
_start:
  .byte 0x80
  .value 0x8000
  .long 0xffffffff
  .reloc 0, R_386_8, target
  .reloc 1, R_386_16, target
  .reloc 3, R_386_32, target
target:
  .byte 0
.size _start, .-_start
