SECTIONS {
  .foo   : { *(.text.foo) }
  .tbss1 : { *(.tbss.tbss1) }
  .tbss2 : { *(.tbss.tbss2) }
  .data  : { *(.data.data) }
}
