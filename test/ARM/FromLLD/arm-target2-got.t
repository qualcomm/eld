SECTIONS {
  .ARM.exidx 0x10114 : { *(.ARM.exidx) }
  .ARM.extab 0x10124 : { *(.ARM.exidx) }
  .rodata 0x10130 : { *(.rodata*) }
  .text 0x20134 : { *(.text*) }
  .got 0x3013c: { *(.got) }
}
