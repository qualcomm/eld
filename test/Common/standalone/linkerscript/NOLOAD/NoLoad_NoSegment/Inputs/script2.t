PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .bar (NOLOAD) : { *(.text.bar) }
  .baz (NOLOAD) : { *(.text.baz) }
  .bad  : { *(.text.bad) } :B
  /DISCARD/ : { *(.ARM.exidx*) }
}
