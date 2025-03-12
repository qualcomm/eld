SECTIONS {
  __start_MYTEXT = .;
  MYTEXT : {
    *(.text*)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
