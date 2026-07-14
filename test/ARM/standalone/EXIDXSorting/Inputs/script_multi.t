SECTIONS {
  .text (0x11000) : { *(.text.fa2) *(.text.fa1) *(.text.fb2) *(.text.fb1) }
  .ARM.exidx : { *(.ARM.exidx*) }
}
