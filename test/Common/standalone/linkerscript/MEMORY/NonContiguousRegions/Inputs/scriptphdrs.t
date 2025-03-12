PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
}

MEMORY {
  M1 : ORIGIN = 0x8000, LENGTH = 4K
  M2 : ORIGIN = 0x2000, LENGTH = 4K
}

SECTIONS {
  .foo : { *(.text.foo) *(.text) } > M1 :A
  .bar : AT (0xf000) { *(.text.bar) } > M2 :B
  .baz : { *(.text.baz) } > M1 :C
  /DISCARD/ : { *(.ARM.exidx*) *(.*.attributes) }
}

