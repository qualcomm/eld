SECTIONS {
  .foo    : { *(.text.foo) }
  .empty1 : {}
  .tbss   : { *(.tbss.tbss) }
  .empty2 : {}
  .data   : { *(.data.data) }
}
