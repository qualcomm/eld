  .section .head,"ax",@progbits
  .globl head_dummy
head_dummy:
  ret
  nop
  .globl head_start
head_start:
  call  head_dummy
  .balign 4
  .globl aligned_a
aligned_a:
  ret
  nop
  call  head_dummy
  .balign 4
  .globl aligned_b
aligned_b:
  ret
