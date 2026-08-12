  .section .text.calls,"ax",@progbits
  .globl dummy_target
dummy_target:
  ret
  .rept 7
  call dummy_target
  .endr
  .align 3
