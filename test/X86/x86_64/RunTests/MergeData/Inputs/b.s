  .section .rodata.cst4,"aM",@progbits,4
  .globl cst_b
cst_b:
  .long 0x4048f5c3

  .text
  .globl get_b
get_b:
  leaq cst_b(%rip), %rax
  ret
