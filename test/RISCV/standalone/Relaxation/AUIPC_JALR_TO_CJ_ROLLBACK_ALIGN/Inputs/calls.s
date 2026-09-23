  .section .text.calls,"ax",@progbits
  .globl dummy_target
dummy_target:
  ret
  .rept 32
  call dummy_target
  .endr
