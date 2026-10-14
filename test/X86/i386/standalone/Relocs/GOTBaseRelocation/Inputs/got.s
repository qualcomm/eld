.section .text,"ax",@progbits
.globl _start
.type _start,@function
_start:
  .long 5
  .long 6
  .reloc 0, R_386_GOTOFF, target
  .reloc 4, R_386_GOTPC, _GLOBAL_OFFSET_TABLE_
target:
  .byte 0
.size _start, .-_start

.section .got,"aw",@progbits
  .long 0
