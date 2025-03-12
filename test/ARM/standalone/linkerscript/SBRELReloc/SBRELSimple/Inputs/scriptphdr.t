PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
  D PT_ARM_EXIDX;
}

SECTIONS {
  .text : { *(.text) } :A
  .data : { *(.data.bar) } :B
  .bss : { *(COMMON) } :B
  .ARM.exidx : { *(.ARM.exidx*) } :C :D
}
