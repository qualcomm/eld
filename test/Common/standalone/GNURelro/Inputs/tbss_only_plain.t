SECTIONS {
  .text  : { *(.text*) *(.eh_frame*) }
  .tbss  : { *(.tbss*) }
  .data  : { *(.data*) }
}
