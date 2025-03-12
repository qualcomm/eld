.globl _start
_start:
  nop

.section .text.foo
.globl foo
foo:
  .word 0x5678

.data
  .word .text.foo
