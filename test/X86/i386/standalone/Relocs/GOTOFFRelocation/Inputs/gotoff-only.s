.section .text,"ax",@progbits
.globl _start
.type _start,@function
_start:
  .long 5
  .reloc 0, R_386_GOTOFF, target
target:
  .byte 0
.size _start, .-_start
