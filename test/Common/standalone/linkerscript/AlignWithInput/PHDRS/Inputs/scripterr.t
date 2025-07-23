SECTIONS {
  .foo : ALIGN(0x20) ALIGN_WITH_INPUT  { *(.text.foo) }
  .bar : ALIGN(0x20) ALIGN_WITH_INPUT  { *(.text.bar) }
  .baz : ALIGN(0x20) ALIGN_WITH_INPUT { *(.text.baz) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
