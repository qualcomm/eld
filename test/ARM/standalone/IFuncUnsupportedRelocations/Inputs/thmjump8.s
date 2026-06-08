// Reference an IFUNC symbol with R_ARM_THM_JUMP8 (narrow conditional Thumb
// branch, +/-256B range). This is an UNSUPPORTED use: R_ARM_THM_JUMP8 belongs
// to the control-flow category but eld cannot apply it to reach an IFunc PLT.

  .syntax unified
  .arch armv7-a
  .thumb
  .text
  .global ref
  .thumb_func
ref:
  .reloc ., R_ARM_THM_JUMP8, foo
  nop
