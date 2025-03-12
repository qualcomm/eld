SECTIONS {
  . = 0x80000000;
  .foo : AT(0) { *(.text.foo) }
  .bar : AT(16) { *(.text.bar) }
}
