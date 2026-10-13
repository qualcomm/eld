  .section .rodata.cst4,"aM",@progbits,4
  .globl cst_a
cst_a:
  .long 0x4048f5c3

  .text
  .globl get_a
get_a:
  leaq cst_a(%rip), %rax
  ret
