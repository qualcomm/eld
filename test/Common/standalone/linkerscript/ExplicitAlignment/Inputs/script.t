SECTIONS {
  .text : { *(.text.bar) }
  .data : ALIGN(32) { *(.text.foo) *(.text.baz) }
}
