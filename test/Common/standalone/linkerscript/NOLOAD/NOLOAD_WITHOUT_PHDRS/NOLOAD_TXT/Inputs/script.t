SECTIONS {
  .foo : { *(.text.foo) }
  .bar (NOLOAD) : { *(.text.bar) }
  .baz (NOLOAD) : { *(.text.baz) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.riscv.attributes) }
}
