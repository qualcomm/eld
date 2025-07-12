SECTIONS {
  . = 0x1000 ^ 0x0100;
  .text : { *(.text) }
}
