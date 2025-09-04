SECTIONS {
  bar : { *(.text.bar) }
  .text : { *(.text*) }
}
