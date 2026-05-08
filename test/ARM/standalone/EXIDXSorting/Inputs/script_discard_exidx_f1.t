SECTIONS {
  .text (0x11000) : { *(.text.f2) *(.text.f1) *(.text.start) }
  /DISCARD/ : { *(.ARM.exidx.text.f1) }
  .ARM.exidx : { *(.ARM.exidx*) }
}
