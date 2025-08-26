SECTIONS {
  .text (0x14) : {
    *(.nonalloc)
    *(*text*)
  }
}