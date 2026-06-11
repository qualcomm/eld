SECTIONS {
  .foo   : { *(.text.foo) }
  .empty : {}
  .tbss  : { *(.tbss.tbss) }
  .data  : { *(.data.data) }
}
