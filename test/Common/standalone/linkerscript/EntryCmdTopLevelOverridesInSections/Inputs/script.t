ENTRY(bar)
SECTIONS {
  ENTRY(foo)
  . = 0x1000;
  .bar : { *(.text.bar) }
  .foo : { *(.text.foo) }
}
