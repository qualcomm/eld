ENTRY(foo)
SECTIONS {
  . = 0x1000;
  .foo : { *(.text.foo) }
}
