// Take the address of the IFUNC `foo` via a PC-relative MOVW/MOVT pair
// (position-independent): emits R_ARM_MOVW_PREL_NC + R_ARM_MOVT_PREL.

  .arch armv7-a
  .text
  .global get_foo_from_outside
  .type get_foo_from_outside, %function

get_foo_from_outside:
  movw r0, #:lower16:(foo - (.LPC0 + 8))
  movt r0, #:upper16:(foo - (.LPC0 + 8))
.LPC0:
  add r0, pc, r0
  bx lr

  .size get_foo_from_outside, .-get_foo_from_outside
