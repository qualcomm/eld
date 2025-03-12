SECTIONS {
  MYTEXT : {
    __start_MYTEXT = .;
    *(.text*)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
