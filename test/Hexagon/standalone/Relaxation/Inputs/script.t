SECTIONS {
  .foo : { *(.text.?oo)  }
  . = 0xF0000000;
  .bar : { *(.text.baz) *(.text.bar) }
}
