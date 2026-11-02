ENTRY(bar)
ENTRY(foo)
ENTRY(bar)
SECTIONS {
  . = 0x1000;
  .bar : { *(.text.bar) }
  . = 0x2000;
  .foo : { *(.text.foo) }
}
