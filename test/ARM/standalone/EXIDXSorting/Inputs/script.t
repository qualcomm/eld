SECTIONS {
  .text (0x11000) : { *(.text.f2) *(.text.f1) *(.text.start) }
  .ARM.exidx : { *(.ARM.exidx*) }
}
