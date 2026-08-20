SECTIONS {
  ENTRY(foo)
  . = 0x1000;
  .foo : { *(.text.foo) }
  .bar : { *(.text.bar) }
}
