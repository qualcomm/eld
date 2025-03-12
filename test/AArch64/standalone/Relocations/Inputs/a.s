// R_AARCH64_ADD_ABS_LO12_NC
.section .text.bar,"ax",@progbits
.p2align 4
  adrp    x0, foo                 // R_AARCH64_ADR_PREL_PG_HI21
  add     x0, x0, :lo12:foo       // R_AARCH64_ADD_ABS_LO12_NC
  blr     x0
.section .text.foo,"ax",@progbits
.type foo,@function
.global foo
foo:
ret
