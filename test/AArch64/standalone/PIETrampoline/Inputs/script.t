SECTIONS {
  . = 0x0;
  .foo : { *(.text.bar) }
  . = 0xF00000000;
  .bar : { *(.text.foo) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
