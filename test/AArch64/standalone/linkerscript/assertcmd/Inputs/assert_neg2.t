SECTIONS {
  sym1 = .;
  .section1 : { *(.text)
  }
  sym2 = .;
  .section2 : { *(*) }
}

ASSERT(SIZEOF(.section1) == 0, "assert for script assert")
