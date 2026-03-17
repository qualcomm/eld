  .section .text,"ax",%progbits
  .global get_foo
  get_foo:
    movz x0, #:abs_g0_nc:foo
    movk x0, #:abs_g1_nc:foo
    movk x0, #:abs_g2_nc:foo
    movk x0, #:abs_g3:foo
    ret
