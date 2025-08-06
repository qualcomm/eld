PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
  T1 PT_TLS;
}

SECTIONS {
  .text (0x1000) : { *(.text*) } :A
  .data : { *(.data*) } :B
  .tdata (0x4000) : { *(.tdata*) } :B :T1
  .tbss : { *(.tbss*) } :C :T1
}