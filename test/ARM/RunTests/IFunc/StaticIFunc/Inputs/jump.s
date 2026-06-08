// Tail-call the IFUNC `foo` via an unconditional branch: emits R_ARM_JUMP24.

  .text
  .global call_foo_from_outside
  .type call_foo_from_outside, %function

call_foo_from_outside:
  b foo

  .size call_foo_from_outside, .-call_foo_from_outside
