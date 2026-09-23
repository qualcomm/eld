  .section .exit.text,"ax",@progbits
  .globl exit_func
exit_func:
  call kobject_put
  nop
  .balign 8
  .globl aligned_label
aligned_label:
  ret
