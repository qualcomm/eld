ENTRY(foo)
SECTIONS {
  . = 0x1000;
  .foo : { *(.text.foo) }
  . = 0x2000;
  .bar : { *(.text.bar) }
}
