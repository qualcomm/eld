PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .bar (NOLOAD) : { *(.bss.bar) } :B
  .baz (NOLOAD) : { *(.bss.baz) } :B
}
