SECTIONS {
  .text 0x1000 : { *(.text.live) }
  .text.live2 0x2000 : { *(.text.live2) }
  .text.dead : { *(.text.dead) }
  .debug_abbrev 0 : { *(.debug_abbrev) }
  .debug_info 0 : { *(.debug_info) }
  .debug_ranges 0 : { *(.debug_ranges) }
}
