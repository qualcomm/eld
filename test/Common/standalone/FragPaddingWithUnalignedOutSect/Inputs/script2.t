SECTIONS {
  .text (0x14) : {
    . = . + 0x2;
    *(.text.foo)
    *(*text*)
  }
}