SECTIONS {
  ENTRY(foo)
  . = 0x1000;
  .foo : { *(.text.foo) }
  .unused : { *(.text.unused) }
}
