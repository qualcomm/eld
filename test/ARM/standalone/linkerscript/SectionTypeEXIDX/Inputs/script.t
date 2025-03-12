SECTIONS {
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .ARM.extab : { *(.ARM.extab*) }
  .data : { *(.data*) }
  .myexidx : { *(.ARM.exidx*) }
  .bss : { *(.bss*) }
}
