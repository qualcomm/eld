SECTIONS {
  INCLUDE text.t
  .data : { *(COMMON) *(.sdata*) }
}
