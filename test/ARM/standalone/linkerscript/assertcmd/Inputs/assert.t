ASSERT(1, "should not assert for script assert")
__sym_1 = ASSERT(1, "should not assert for script assert");

SECTIONS {
  sym1 = .;
  .section1 : { *(.text)
    ASSERT(1 , "should not assert for output section assert")
    __sym_2 = ASSERT(1, "should not assert for output section assert");
  }
  ASSERT(SIZEOF(.section1) > 0, "should not assert for section assert")
  sym2 = .;
  t = 0x123;
  ASSERT(t==0x123, "should not assert for t")
  t = 0x456;
  __assert_sink__ = ASSERT(sym1 < sym2, "should not assert");
  __assert_sink__ = ASSERT(sym1 > sym2, "sym1 smaller then sym2");
  .section2 : { *(*) }
}
