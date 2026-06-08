// Tail-call the IFUNC `foo` from Thumb code via a wide unconditional branch:
// emits R_ARM_THM_JUMP24.

  .syntax unified
  .arch armv7-a
  .thumb
  .text
  .global call_foo_from_outside
  .thumb_func
  .type call_foo_from_outside, %function

call_foo_from_outside:
  b.w foo

  .size call_foo_from_outside, .-call_foo_from_outside
