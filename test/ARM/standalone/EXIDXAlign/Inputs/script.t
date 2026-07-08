SECTIONS {
  .text (0x11000) : { *(.text.f2) *(.text.f1) *(.text.start) }
  /* Place .ARM.exidx at a 4-byte-aligned but not 8-byte-aligned address so
     that the '. = ALIGN(0x8)' below must shift the exidx fragments by 4. */
  .ARM.exidx (0x11014) : {
    . = ALIGN(0x8);
    PROVIDE(__exidx_start = .);
    *(.ARM.exidx*)
    PROVIDE(__exidx_end = .);
  }
}
