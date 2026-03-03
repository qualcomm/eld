SECTIONS {
  .text_nobar : { EXCLUDE_FILE(*bar.o) *(.text*) }
  .text_bar   : { *(.text*) }
  .data : { *(.data*) }
  .bss  : { *(.bss*)  }
}
