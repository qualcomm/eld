SECTIONS {
  ENTRY(unknown_symbol)
  . = 0x1000;
  .text : { *(.text*) }
}
