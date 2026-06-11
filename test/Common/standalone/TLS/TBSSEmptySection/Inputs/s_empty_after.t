SECTIONS {
  .foo   : { *(.text.foo) }
  .tbss  : { *(.tbss.tbss) }
  .empty : {}
  .data  : { *(.data.data) }
}
