  .section .text,"ax",@progbits
  .global get_foo_from_outside
  .type get_foo_from_outside, @function

get_foo_from_outside:
.Lgot_hi0:
  auipc a0, %got_pcrel_hi(foo)
  ld a0, %pcrel_lo(.Lgot_hi0)(a0)
  ret

  .size get_foo_from_outside, .-get_foo_from_outside
