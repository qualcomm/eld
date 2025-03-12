SECTIONS {
  . = 0x80000000;
  .foo : AT(ADDR(.foo) - 0) { *(.text.foo) }
  .bar : AT(ADDR(.bar) - 0) { *(.text.bar) }
}
