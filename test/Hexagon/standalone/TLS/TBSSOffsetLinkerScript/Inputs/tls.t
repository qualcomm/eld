SECTIONS {
  .tdata : { *(.tdata.*) }
  .tbssA : { *(.tbss.b) }
  .tbssB : { *(.tbss.c) }
  .data :  { *(.data*) }
}
