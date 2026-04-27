  .section .text,"ax",%progbits
  .global _start
_start:
  ldr x0, :got:foo
  ret