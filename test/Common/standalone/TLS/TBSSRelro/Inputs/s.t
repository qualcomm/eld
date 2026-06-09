SECTIONS {
  .foo  : { *(.text.foo) }
  .tbss : { *(.tbss.tbss) }
  .data : { *(.data.data) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
