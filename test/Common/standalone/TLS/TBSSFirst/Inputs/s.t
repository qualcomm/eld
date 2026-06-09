SECTIONS {
  .tbss : { *(.tbss.tbss) }
  .foo  : { *(.text.foo) }
  .data : { *(.data.data) }
  /DISCARD/ : { *(.ARM.exidx*) *(.eh_frame*) }
}
