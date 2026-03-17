  .section .text,"ax",%progbits
  .global get_foo
  get_foo:
    ldr x0, foo
    ret
