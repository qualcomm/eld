SECTIONS {
  ENTRY(foo)
  ENTRY(baz)
  . = 0x1000;
  .foo : { *(.text.foo) }
  .baz : { *(.text.baz) }
}
