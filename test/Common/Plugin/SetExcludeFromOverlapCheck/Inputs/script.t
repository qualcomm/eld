SECTIONS {
  . = 0x8000;
  .sec1 : AT(0x9000) { *(.sec1) }
  . = 0x8000;
  .sec2 : AT(0xA000) { *(.sec2) }
}
