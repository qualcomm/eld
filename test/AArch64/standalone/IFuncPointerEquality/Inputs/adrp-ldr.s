  .section .text,"ax",%progbits
  .global get_foo_from_outside
  .type get_foo_from_outside, %function

get_foo_from_outside:
  adrp x0, :got:foo
  ldr x0, [x0, #:got_lo12:foo]
  ret

  .size get_foo_from_outside, .-get_foo_from_outside