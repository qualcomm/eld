SECTIONS {
  .text (0x1233000) : {
    *(.text)
  }
  .rodata (0x1234560) : {
    *(.rodata)
  }
}
