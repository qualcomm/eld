PHDRS {
  p1 PT_LOAD;
  p2 PT_LOAD;
}
SECTIONS {
  .text :
  {
    *(.text.foo)
    *(.text.bar)
  } :p1
  .bss : {
    *(*.bss)
  } :p2
  .comment : {
    *(*.comment)
  }
  .allnote : {
    *(.note*)
  }
  .hexagon.attributes : {*(.hexagon.attributes)}
  __unrecognized_start__ = . ;
  .unrecognized : { *(*) }
  __unrecognized_end__ = . ;

  __assert_sink__ = ASSERT( __unrecognized_end__ == __unrecognized_start__ , "Unrecognized sections - see linker script" );
}
