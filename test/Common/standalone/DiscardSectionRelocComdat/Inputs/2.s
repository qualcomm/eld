.globl _start
_start:
  nop

.section .text.foo,"axG",%progbits,foo,comdat
.globl foo
foo:
  .word 0x5678

.data
  .word .text.foo
