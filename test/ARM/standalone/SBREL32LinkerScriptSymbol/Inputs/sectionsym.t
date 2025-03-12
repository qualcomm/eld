SECTIONS {
  MYTEXT : {
    *(.text*)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
