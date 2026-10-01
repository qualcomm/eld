SECTIONS {
  .text 0x1000 : { *(.text) }
  .got.plt 0x2000 : { *(.got.plt) }
}
