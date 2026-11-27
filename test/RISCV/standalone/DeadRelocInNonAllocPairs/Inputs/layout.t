SECTIONS {
  .text 0x10 : { *(.text.live) *(.text.dead) }
  .debug_pairs32 0 : { *(.debug_pairs32) }
  .debug_pairs_uleb 0 : { *(.debug_pairs_uleb) }
  /DISCARD/ : { *(.discarded) }
}
