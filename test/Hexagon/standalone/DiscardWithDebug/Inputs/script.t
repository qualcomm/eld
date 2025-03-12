SECTIONS {
  .foo : { *(.text.foo) }
  .bar : { *(.text.bar) }
  /DISCARD/ : { *(.text.baz) }
}
