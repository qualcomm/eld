PHDRS {
  load PT_LOAD;
  exidx PT_ARM_EXIDX;
}

SECTIONS {
  .text : { *(.text*) } :load
  .rodata : { *(.rodata*) } :load
  .ARM.extab : { *(.ARM.extab*) } :load
  .data : { *(.data*) } :load
  .ARM.exidx : { *(.ARM.exidx*) } :load :exidx
  .bss : { *(.bss*) } :load
}
