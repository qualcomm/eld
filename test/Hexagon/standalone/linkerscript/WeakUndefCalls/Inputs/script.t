SECTIONS {
  . = 0x1000;
  .text : {
    *(.text.guard)
  }
  . = 0xF0000000;
  .bar : {
    *(.text.bar)
  }
}
