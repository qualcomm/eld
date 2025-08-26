SECTIONS {
  .text (0x14) : {
    *(.text.foo)
    *(*text*)
  }
}