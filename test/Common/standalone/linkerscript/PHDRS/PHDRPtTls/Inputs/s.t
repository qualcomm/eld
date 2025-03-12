PHDRS {
  A PT_LOAD;
  B PT_TLS;
}

SECTIONS {
  .text : { *(.text*) } :A
  .tbss : { *(.tbss*) } :B
}
