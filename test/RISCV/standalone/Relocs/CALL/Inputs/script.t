SECTIONS {
  .text (0x1234000) : {
    *(.text.foo)
  }
  .secondtxt (0x1234560) : {
  *(.text.bar)
  }
}
