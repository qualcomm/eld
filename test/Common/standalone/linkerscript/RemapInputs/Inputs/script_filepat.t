SECTIONS {
  .text_bar  : { *bar.o(.text*) }
  .text_rest : { *(.text*) }
  .data      : { *(.data*) }
  .bss       : { *(.bss*)  }
}
