SECTIONS {
  .text : {
    . = . + 100;
    *(.text)
    . = ALIGN(8);
    sym = .;
  }
  sym_after_text = .;
  . = ALIGN(1M);
  myend = .;
  . = . + 4;
  ASSERT(sym_after_text < ADDR(.data), "Bad Ugly")
}
