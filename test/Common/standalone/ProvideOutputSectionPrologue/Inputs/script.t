SECTIONS {
  PROVIDE(VMA = 0x1000);
  PROVIDE(LMA = 0x2000);
  PROVIDE(A = 0x100);
  PROVIDE(S = 0x100);
  .foo (VMA) : AT(LMA) ALIGN(A) SUBALIGN(S) {
    *(.text.foo)
  }
}