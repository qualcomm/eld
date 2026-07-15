SECTIONS {
  .ARM.exidx 0x1000 : { *(.ARM.exidx*) foo = .; }
  .text      0x2000 : { *(.text*) }
}
