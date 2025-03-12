MEMORY {
  M1 : ORIGIN = 0x1000, LENGTH = 4K
  M2 : ORIGIN = 0x2000, LENGTH = 4K
}

SECTIONS {
  .foo : { *(.text.foo) *(.text) } > M1 AT> M1
  .bar : AT (0xf000) { *(.text.bar) } > M2 AT> M2
  .baz : { *(.text.baz) } > M1 AT > M1
  /DISCARD/ : { *(.ARM.exidx*) *(.*.attributes) }
}
