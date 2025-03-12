SECTIONS {
  INCLUDE text.t
  mydata = .;
  .data : { *(COMMON) }
  .comment : { *(.comment) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes*) }
}
