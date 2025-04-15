SECTIONS {
  .ARM.exidx 0x100d4 : { *(.ARM.exidx) }
  .ARM.extab 0x100e4 : { *(.ARM.exidx) }
  .rodata 0x100f0 : { *(.rodata*) }
  .text 0x200f4 : { *(.text*) }
}
