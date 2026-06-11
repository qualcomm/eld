SECTIONS {
  .foo  0x1000 : { *(.text.foo) }
  .tbss 0x3000 : { *(.tbss.tbss) }
  .data        : { *(.data.data) }
}
