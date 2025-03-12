SECTIONS {
  .text : { *(.text*) }
  .data : { *(.data*) }
  /DISCARD/ : { *(.debug*) }
}
