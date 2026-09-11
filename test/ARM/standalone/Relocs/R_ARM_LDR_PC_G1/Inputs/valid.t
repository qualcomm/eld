SECTIONS {
  .text.1 0x1000000 : AT(0x1000000) { *(.text.1) }
  .text.2 0x1001000 : AT(0x1001000) { *(.text.2) }
}
