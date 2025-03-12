SECTIONS {
  .text (0x1233000) : {
    *(.text.foo)
    *(.text.baz)
  }
  .data (0x12345600) : {
  *(.data)
  }
}
