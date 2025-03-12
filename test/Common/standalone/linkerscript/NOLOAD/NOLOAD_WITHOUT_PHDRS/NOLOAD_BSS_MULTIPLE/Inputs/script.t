SECTIONS {
  .foo : { *(.text.foo) }
  .bar (NOLOAD) : { *(.bss.bar) }
  .baz (NOLOAD) : { *(.bss.baz) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.riscv.attributes) }
}
