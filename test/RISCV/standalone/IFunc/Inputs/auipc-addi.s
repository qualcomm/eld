  .section .text,"ax",%progbits
  .global get_foo_from_outside
  .type get_foo_from_outside, @function

get_foo_from_outside:
.Lpcrel_hi0:
  auipc a0, %pcrel_hi(foo)
  addi a0, a0, %pcrel_lo(.Lpcrel_hi0)
  ret

  .size get_foo_from_outside, .-get_foo_from_outside
