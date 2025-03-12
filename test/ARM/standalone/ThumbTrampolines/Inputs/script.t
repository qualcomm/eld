SECTIONS {
  .foo : { *(.text.foo) }
  . = 2 << 25 + 1;
  .bar : { *(.text.bar) }
  . = 2 << 26 + 1;
  .baz : { *(.text.baz) }
}
