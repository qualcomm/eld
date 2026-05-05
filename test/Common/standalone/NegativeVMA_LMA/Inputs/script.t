SECTIONS {
  .foo (-0x201) : AT(0x1000) { *(.text.foo) }
}