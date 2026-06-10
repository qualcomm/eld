SECTIONS {
  .text : { *(.text*) }
  .data : { *(.data*) *(.sdata*) }
  .bss  : { *(.bss*)  *(.sbss*)  }
}
