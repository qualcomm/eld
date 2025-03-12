PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
  D PT_LOAD;
  TLS PT_TLS;
}

SECTIONS {
  .text : { *(.text*) } :A
  .tdata  : { *(.tdata*) } :TLS :B
  .tbss  : { *(.tbss*) } :TLS :B
  .data : { *(.data*) } :D
}
