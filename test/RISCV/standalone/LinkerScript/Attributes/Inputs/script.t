SECTIONS {
  .comment : { *(.comment) }
  /DISCARD/ : { *(.text*) }
}
