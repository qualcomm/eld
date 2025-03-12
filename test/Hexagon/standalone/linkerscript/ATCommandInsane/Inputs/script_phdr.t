PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
}

SECTIONS {
  .foo (0xf0004000) : AT(0x4000) { *(.text.foo) } :A
  . = ALIGN(256K);
  .boo : {} :B
  .bar : { *(.text.bar) } :B
  . = ALIGN(64K);
  .bak : {} :B
  .baz : { *(.text.baz) }:C
}
