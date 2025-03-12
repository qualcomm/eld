PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : AT(0x1000) { *(.text.foo) }  :A
  .bar : { *(.bss.bss*) }  :A
  .baz : { *(.bss.data*) }  :A
  /DISCARD/ : { *(.ARM.exidx*) *(.eh_frame*) }
}
