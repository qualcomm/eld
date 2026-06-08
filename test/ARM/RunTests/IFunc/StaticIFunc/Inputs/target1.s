// Take the address of the IFUNC `foo` as data via R_ARM_TARGET1, which eld
// treats as R_ARM_ABS32 (a direct reference). Emitted explicitly with .reloc
// since the compiler does not generate R_ARM_TARGET1 directly.

  .text
  .global get_foo_from_outside
  .type get_foo_from_outside, %function

get_foo_from_outside:
  ldr r0, .Lfoo_ptr
  bx lr
.Lfoo_ptr:
  .reloc ., R_ARM_TARGET1, foo
  .word 0

  .size get_foo_from_outside, .-get_foo_from_outside
