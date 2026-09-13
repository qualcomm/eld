SECTIONS {
  .data 0x1000 : {
    . = . + 0x400000000;
    *(.data*)
  }
}
