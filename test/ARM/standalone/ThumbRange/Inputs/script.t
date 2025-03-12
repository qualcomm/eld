SECTIONS {
  .foo : { *(.text.foo) }
  . = 2 << 21 + 1;
  .bar : { *(.text.bar) }
  . = 2 << 24 + 1;
  .baz : { *(.text.baz) }
}
