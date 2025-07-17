SECTIONS {
  .text 0x1000 : AT(0x2000) {
    PROVIDE(__text_start = .);
    *(.text)
    PROVIDE(__text_end = .);
  }
  PROVIDE(__text_size = __text_end - __text_start);
  PROVIDE(__text_loadaddr = LOADADDR(.text));
  PROVIDE(__other_end = __text_end);
}
