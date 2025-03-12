SECTIONS {
  .text :
  {
    *(.text.foo)
    *(.text.bar)
    *(.ARM.exidx*)
    *(.text)
  }
  .bss : {
    *(*.bss)
  }
  .ARM.attributes : {
    *(.ARM.attributes*)
  }
  .comment : {
    *(*.comment)
  }
  __unrecognized_start__ = . ;
  .unrecognized : { *(*) }
  __unrecognized_end__ = . ;

  __assert_sink__ = ASSERT( __unrecognized_end__ == __unrecognized_start__ , "Unrecognized sections - see linker script" );
}
