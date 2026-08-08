  .section .exit.text,"ax",@progbits
  .globl exit_func
exit_func:
  call kobject_put
  ret
