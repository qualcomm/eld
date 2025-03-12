PHDRS {
  A PT_LOAD;
}

SECTIONS {
.A (0x1000) : AT(0) { *(.text.foo) *(.ARM.exidx.text.foo) } :A
.B (0x10000) : AT(1000) { *(.text.bar) *(.ARM.exidx.text.bar) } :A
}
