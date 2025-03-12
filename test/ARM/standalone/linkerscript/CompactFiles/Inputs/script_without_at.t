PHDRS {
  A PT_LOAD;
}

SECTIONS {
.A (0x1000) :  { *(.text.foo) *(.ARM.exidx.text.foo) } :A
.B (0x10000) : { *(.text.bar) *(.ARM.exidx.text.bar) } :A
}
