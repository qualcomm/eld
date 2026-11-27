SECTIONS {
  .text 0x1000 : { *(.text.live) }
  /DISCARD/ : { *(.text.dead) }
}
