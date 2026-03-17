  .section .text,"ax",%progbits
  .global get_foo_from_outside
  .type get_foo_from_outside, %function

get_foo_from_outside:
  adr x0, foo
  ret

  .size get_foo_from_outside, .-get_foo_from_outside