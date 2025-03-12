SECTIONS {
  .text : { *(.text) }
  /DISCARD/ : { *(.text.foo) }
}