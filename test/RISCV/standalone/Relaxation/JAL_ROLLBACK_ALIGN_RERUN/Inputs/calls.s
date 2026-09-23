  .section .text.calls,"ax",@progbits
  .globl dummy_target
dummy_target:
  ret
  .rept 256
  call dummy_target
  .endr
