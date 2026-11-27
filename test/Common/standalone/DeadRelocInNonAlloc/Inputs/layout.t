SECTIONS {
  .text 0x1000 : { *(.text.live) *(.text.dead) }
  .data 0x2000 : { *(.data) }
  .debug_renamed 0 : { *(.debug_loc) }
  .debug_ranges 0 : { *(.debug_ranges) }
  .debug_other 0 : { *(.debug_other) }
  .debug_info 0 : { *(.debug_info) }
  .metadata 0 : { *(.metadata) }
  /DISCARD/ : { *(.discarded) }
}
___end = 0x11111111;
absolute = 0x2000;
