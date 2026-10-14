SECTIONS {
  .text 0x1000 : { *(.text) }
  .got 0x2000 : { *(.got) }
}
