SECTIONS {
  .got (0x90000) : {
    QUAD(0xdeadc00c)
    *(.got.plt)
  }
}
