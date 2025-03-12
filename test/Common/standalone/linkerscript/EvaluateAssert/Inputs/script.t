SECTIONS {
  .foo : { *(.text.foo) }
  ASSERT((. + 1) > ., "Dot + 1 should always be greater than Dot")
  .bar : { *(.text.bar) }
  ASSERT(. == ., "Dot should always be equal to .");
  ASSERT(SIZEOF(.foo) >= 0, "Wow!")
  .comment : { *(.comment) }
  /* Group all remaining sections here, and generate an error if there are any. */
  __unrecognized_start__ = . ;
  .unrecognized : { *(*) }
  __unrecognized_end__ = . ;
  ASSERT(__unrecognized_end__ == __unrecognized_start__ , "Unrecognized sections - see linker script")
}
