SECTIONS {
  .text.1 0x0800000 : AT(0x0800000) { *(.text.1) }
  .text.2 0xf0f0000 : AT(0xf0f0000) { *(.text.2) }
}
