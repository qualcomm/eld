SECTIONS {
  .foo : { *(.text.foo) }
  .tbss : { *(.tbss.tbss) }
  .data : { *(.data.data) }
}
