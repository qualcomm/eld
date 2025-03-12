
PHDRS {
  INIT PT_LOAD;
  DATA PT_LOAD;
}

SECTIONS {
  .start : { *(.text.foo) }  :INIT
  .ARM.exidx : { *(.ARM.exidx*) }
  .data : { *(.data*) } :DATA
  .rodata (NOLOAD) : { *(.rodata*) }
  .mybar : { *(.text.bar) }
}
