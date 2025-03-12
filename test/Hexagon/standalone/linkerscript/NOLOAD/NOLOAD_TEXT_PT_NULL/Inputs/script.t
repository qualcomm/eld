PHDRS {
  A PT_LOAD;
  B PT_NULL;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .bar (NOLOAD) : { *(.text.bar) } :B
  .baz (NOLOAD) : { *(.bss.baz) } :B
}
