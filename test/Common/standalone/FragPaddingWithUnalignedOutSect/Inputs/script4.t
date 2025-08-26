SECTIONS {
  .text (0x14) : {
    . = . + 0x2;
    *(.nonalloc)
    *(*text*)
  }
}