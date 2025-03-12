SECTIONS {
  .text : { *(.text) }
  .ARM.exidx : { *(.ARM.exidx*) }
  .data : { *(.data.bar) }
  . = . + 8192;
  .bss : { *(COMMON) }
}
