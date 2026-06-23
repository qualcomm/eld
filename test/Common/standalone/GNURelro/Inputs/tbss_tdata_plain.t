SECTIONS {
  .text  : { *(.text*) *(.eh_frame*) }
  .tdata : { *(.tdata*) }
  .tbss  : { *(.tbss*) }
  .data  : { *(.data*) }
}
