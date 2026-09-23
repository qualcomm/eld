PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
}

MEMORY {
  M1     : ORIGIN = 0x1000, LENGTH = 1M
  M1_LMA : ORIGIN = 0x1000, LENGTH = 1M
}

SECTIONS {
  .foo :                { *(.text.foo) } >M1 AT>M1_LMA :A
  .bar ALIGN(0x1000) :  { *(.text.bar) } >M1 AT>M1_LMA :B
  .baz : ALIGN(0x1000)  { *(.text.baz) } >M1 AT>M1_LMA :C
  /DISCARD/ : { *(.ARM.exidx*) *(.*.attributes) *(.eh_frame*) }
}
