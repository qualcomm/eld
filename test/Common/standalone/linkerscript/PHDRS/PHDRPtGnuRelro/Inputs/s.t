PHDRS {
  A PT_LOAD;
  B PT_GNU_RELRO;
}

SECTIONS {
  .text : { *(.text*) } :A :B
}
