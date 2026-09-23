MEMORY {
  M1     : ORIGIN = 0x1000, LENGTH = 1M
  M1_LMA : ORIGIN = 0x1000, LENGTH = 1M
}

SECTIONS {
  .foo :               { *(.text.foo) } >M1 AT>M1_LMA
  .bar ALIGN(0x1000) : { *(.text.bar) } >M1 AT>M1_LMA
  .baz : ALIGN(0x1000) { *(.text.baz) } >M1 AT>M1_LMA
  /DISCARD/ : { *(.ARM.exidx*) *(.*.attributes) *(.eh_frame*) }
}
