SECTIONS {
  .data 0x400001000 : {
    . = 0x100;
    *(.data*)
  }
}
