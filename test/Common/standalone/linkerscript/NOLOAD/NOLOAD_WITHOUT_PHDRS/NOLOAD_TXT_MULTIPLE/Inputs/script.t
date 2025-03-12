SECTIONS {
  .foo : { *(.text.foo) }
  .bar (NOLOAD) : { *(.text.bar) }
  .baz (NOLOAD) : { *(.text.baz) }
  .bad  : { *(.text.bad) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.riscv.attributes) }
}
