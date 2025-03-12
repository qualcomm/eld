PHDRS {
  TEXT PT_LOAD;
}

SECTIONS {
  .text 0 : { *(.text) } :TEXT
  .sdata : { *(.sdata*) *(.scommon*) }
}
