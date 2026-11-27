SECTIONS {
  .text 0x1000 : { *(.text.live) }
  .debug_renamed 0 : { *(.debug_discarded) }
  .metadata 0 : { *(.metadata) }
  /DISCARD/ : { *(.text.discarded) *(.discarded) }
}
absolute = 0x2000;
