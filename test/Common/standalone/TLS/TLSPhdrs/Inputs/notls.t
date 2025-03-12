PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
  D PT_LOAD;
}

SECTIONS {
  .text : { *(.text*) } :A
  .tdata  : { *(.tdata*) } :B
  .tbss  : { *(.tbss*) }  :B
  .data : { *(.data*) } :D
}
