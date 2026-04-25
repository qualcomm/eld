PHDRS {
  A PT_LOAD;
  T PT_TLS;
}

SECTIONS {
  .tdata : ALIGN(0x200) { *(.tdata*) } :A :T
  .tbss : SUBALIGN(0x401) { *(.tbss*) } :A :T
}
