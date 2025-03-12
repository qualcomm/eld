MEMORY {
  b2 (r!) : ORIGIN = 200, LENGTH = 48
}
SECTIONS {
  .t1 : {
     *(.text.*)
  }  >b2
  /DISCARD/ : { *(.ARM*) }
}
