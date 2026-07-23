SECTIONS {
  ENTRY(0x2000)
  . = 0x1000;
  .foo : { *(.text.foo) }
}
