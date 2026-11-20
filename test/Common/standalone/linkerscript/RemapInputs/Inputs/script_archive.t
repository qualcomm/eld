SECTIONS {
  .text_from_bar_archive : { *libbar.a(.text*) }
  .text_rest : { *(.text*) }
  .data : { *(.data*) }
  .bss  : { *(.bss*)  }
}
