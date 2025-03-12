SECTIONS {
  .text : { *(.text) }
  .ARM.exidx : { *(.ARM.exidx*) }
  .data : { *(.data.bar) }
  .bss : { *(COMMON) }
}
