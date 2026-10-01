.section .text,"ax",@progbits
.globl _start
_start:
  .byte 0
  .value 0
  .reloc 1, R_386_PC16, target

.section .target,"a",@progbits
.globl target
target:
  .byte 0
