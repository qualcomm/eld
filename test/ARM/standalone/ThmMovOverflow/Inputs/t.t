PHDRS {
  MON_CODE_LR PT_LOAD;
  MON_DATA_LR PT_LOAD;
  MON_UNCACHED_DATA_LR PT_LOAD;
}
SECTIONS {
  MON_CODE_ER ((0x80000000 + 0x01600000) + 0x0): {
    Image$$MON_CODE_ER$$Base = .;
    KEEP(* (.code.* .text))
    KEEP(*(.rodata .rodata.*))
  } : MON_CODE_LR
  Image$$MON_CODE_ER$$Length = SIZEOF(MON_CODE_ER);
  MON_DATA_ER ALIGN(4096) : {
     Image$$MON_RW_DATA_ER$$Base = .;
     *(.data .bss .common COMMON)
     KEEP(* (.data.*))
  } : MON_DATA_LR
  Image$$MON_RW_DATA_ER$$Length = SIZEOF(MON_DATA_ER);
   MON_UNCACHED_DATA_ER ALIGN(4096): {
      Image$$MON_UNCACHED_DATA_ER$$Base = .;
      Image$$MON_EL1_SHARED_DATA_ER$$Base = .;
      KEEP(* (.uncached.*))
      Image$$MON_EL1_SHARED_DATA_ER$$Limit = .;
   } : MON_UNCACHED_DATA_LR
   Image$$MON_UNCACHED_DATA_ER$$Length = SIZEOF(MON_UNCACHED_DATA_ER);
  .debug 0 : { *(.debug) }
  .line 0 : { *(.line) }
  .debug_srcinfo 0 : { *(.debug_srcinfo) }
  .debug_sfnames 0 : { *(.debug_sfnames) }
  .debug_aranges 0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev 0 : { *(.debug_abbrev) }
  .debug_line 0 : { *(.debug_line) }
  .debug_frame 0 : { *(.debug_frame) }
  .debug_str 0 : { *(.debug_str) }
  .debug_loc 0 : { *(.debug_loc) }
  .debug_macinfo 0 : { *(.debug_macinfo) }
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames 0 : { *(.debug_varnames) }
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges 0 : { *(.debug_ranges) }
  .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
  .note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) }
  /DISCARD/ : { *(.ARM.exidx*) *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.init) *(.fini) }
}
