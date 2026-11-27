SECTIONS {
  .text 0x20 : { *(.text.live) *(.text.dead) }
  .debug_pair 0 : { *(.debug_pair) }
  /DISCARD/ : { *(.discarded) }
}
