SECTIONS {
  .text.foo : {
    . = . + 100;
    *(.text.foo)
    . = ALIGN(8);
    sym = .;
  }
  sym_after_text = .;
  . = ALIGN(1M);
  myend = .;
  . = . + 4;
  .data : { *(.data) }
  ASSERT(sym_after_text < ADDR(.data), "Bad Ugly")
}
