SECTIONS {
  .foo : { *(.text.foo) }
  .bar (NOLOAD) : { *(.bss.bar) }
  .baz  : { *(.bss.baz) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.riscv.attributes) }
}
