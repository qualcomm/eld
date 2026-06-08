// Call the IFUNC `foo` from Thumb code: emits R_ARM_THM_CALL.

  .arch armv7-a
  .syntax unified
  .thumb
  .text
  .global call_foo_from_outside
  .thumb_func
  .type call_foo_from_outside, %function

call_foo_from_outside:
  push {lr}
  bl foo
  pop {pc}

  .size call_foo_from_outside, .-call_foo_from_outside
