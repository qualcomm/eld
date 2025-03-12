SECTIONS {
  .text : { *(.text*) }
  .tbss : { *(.tbss*) }
  . = ALIGN(4K);
  .data : { *(.data*) }
}
