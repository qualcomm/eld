SECTIONS {
  .interp  : { *(.interp) }
  .dynsym  : { *(.dynsym) }
  .dynstr  : { *(.dynstr) }
  .hash    : { *(.hash)   }
  .tdata   : { *(.tdata.*) }
  .tbssA   : { *(.tbss.b) }
  .tbssB   : { *(.tbss.c) }
  .dynamic : { *(.dynamic) }
  .got.plt : { *(.got.plt) }
  .data    : { *(.data*) *(.sdata*) }
  .comment : { *(.comment) }
}
