SECTIONS {
  .text (0x1233000) : {
    *(.text.foo)
  }
  .anothertext (0x1234560) : {
    *(.text.bar)
  }
}
