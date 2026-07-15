SECTIONS {
  . = 0x10000;
  .text : { *(.text) }
  /DISCARD/ : { *(.exit.text) }
}
