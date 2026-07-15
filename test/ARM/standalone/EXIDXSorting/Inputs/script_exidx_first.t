SECTIONS {
  .ARM.exidx : { *(.ARM.exidx*) }
  .text.fa 0x2000 : { *(.text.fa*) }
  .text.fb 0x1000 : { *(.text.fb*) }
}
