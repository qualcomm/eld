SECTIONS {
  .ro : ONLY_IF_RO { *(.x*) }
  .rw : ONLY_IF_RW { *(.x*) }
}
