MEMORY {
  b1 (r!x) : ORIGIN = 200, LENGTH = 48
}
SECTIONS {
  .t1 : {
     *(.text.*)
  }
  /DISCARD/ : { *(.ARM*) }
}
