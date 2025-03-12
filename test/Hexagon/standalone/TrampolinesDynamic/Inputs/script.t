SECTIONS {
  .text : { *(.text) }
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .interp : { *(.interp) }
  .hash : { *(.hash) }
  .rela.plt : { *(.rela.plt) }
  .got : { *(.got) }
  .got.plt : { *(.got.plt) }
  .bss : { *(.bss) }
}
