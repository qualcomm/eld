SECTIONS {
.text : { *(.text.bar)  *(.text.baz) }
  /DISCARD/ : { *(.text.foo) }
}
