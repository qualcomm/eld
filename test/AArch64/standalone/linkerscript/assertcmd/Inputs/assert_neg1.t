ASSERT(1, "should not assert for script assert");
SECTIONS {
  sym1 = .;
  .section1 : { *(.text)
  }
  sym2 = .;
  __assert_sink__ = ASSERT(sym1 < sym2, "should not assert");
  __assert_sink__ = ASSERT(sym1 > sym2, "sym1 smaller then sym2");
  .section2 : { *(*) }
}
