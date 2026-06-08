// Take the address of the IFUNC `foo` via a MOVW/MOVT pair forming the
// absolute address: emits R_ARM_MOVW_ABS_NC + R_ARM_MOVT_ABS. Valid only
// in a non-PIC link.

  .arch armv7-a
  .text
  .global get_foo_from_outside
  .type get_foo_from_outside, %function

get_foo_from_outside:
  movw r0, #:lower16:foo
  movt r0, #:upper16:foo
  bx lr

  .size get_foo_from_outside, .-get_foo_from_outside
