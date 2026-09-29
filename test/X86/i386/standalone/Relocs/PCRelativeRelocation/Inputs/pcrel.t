SECTIONS {
  .text 0 : { *(.text) }
  .target 0x8001 : { *(.target) }
}
