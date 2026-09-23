PHDRS {
  seg0 PT_LOAD FLAGS(7);
  seg1 PT_LOAD FLAGS(7);
}
SECTIONS {
  .foo 0x1000 : { *(.text.foo) } :seg0
  .bar 0x2000 : { *(.text.bar) } :seg1
}
