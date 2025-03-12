SECTIONS {
  . = 0xffffff00;
  .text : { *(.text*) }
  .debug_loc 0 : { *(.debug_loc) }
  .debug_abbrev 0 : { *(.debug_abbrev) }
  .debug_info 0 : { *(.debug_info) }
  .debug_ranges 0 : { *(.debug_ranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
}
