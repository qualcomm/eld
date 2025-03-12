SECTIONS {
  .data : { *(.data*) }
  .rela.dyn : { *(.rela.dyn) }
  /DISCARD/ : { *(.dynsym) }
}
