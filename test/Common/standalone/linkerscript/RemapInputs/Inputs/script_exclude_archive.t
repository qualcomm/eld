SECTIONS {
  .text_nobar : { EXCLUDE_FILE(*libbar.a) *(.text*) }
  .text_bar   : { *(.text*) }
  .data : { *(.data*) }
  .bss  : { *(.bss*)  }
}
