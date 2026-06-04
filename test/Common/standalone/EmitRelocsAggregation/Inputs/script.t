SECTIONS {
  CODE : { *(.text mycode*) }
  DATA : { *(.data .data.* .bss mydata) }
}
