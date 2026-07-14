SECTIONS {
  /* f2 placed before f1 in .text so f2 has the lower address.
   * The catch-all EXIDX rule picks up both sections; sortEXIDX() must order
   * them by function address (f2 first, f1 second). */
  .text (0x11000) : { *(.text._Z2f2v) *(.text._Z2f1v) }
  .ARM.exidx : { *(.ARM.exidx*) }
}
