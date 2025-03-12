SECTIONS {
  .text (0x10000) : {
    *(.text*)
    *(.text*)
  }
  .data (0x15000) : {
    *(.data*)
  }
}
