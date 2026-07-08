SECTIONS {
  .text (0x10000) : { *(.text*) }
  .debug_info : { *(.debug_info*) }
  after_debug = .;
  .data : { *(.data*) }
}
