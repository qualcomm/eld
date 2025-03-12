SECTIONS {
  sym1 = .;
  .section1 : { *(.text)
  }
  sym2 = .;
  .section2 : { *(*) }
  ASSERT(sym1 > sym2, "sym1 smaller then sym2")
}
