SECTIONS {
  .text.1 0x0800000 : AT(0x0800000) { *(.text.1) }
  .text.2 0x28000000 : AT(0x28000000) { *(.text.2) }
}
