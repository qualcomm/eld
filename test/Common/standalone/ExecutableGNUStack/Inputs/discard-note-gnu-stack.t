SECTIONS {
  .text : { *(.text) }
  /DISCARD/ : { *(.note.GNU-stack) }
}
