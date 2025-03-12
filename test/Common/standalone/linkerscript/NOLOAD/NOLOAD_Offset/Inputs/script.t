PHDRS {
  CODE PT_LOAD;
  MYNULL PT_NULL;
}
SECTIONS {
  .code : {
    *(.text.foo)
  } :CODE
  .noload (NOLOAD) : {
    *(.bss*)
    . = ALIGN(64);
  } :MYNULL
  .comment : {
    *(.comment)
  }
  /DISCARD/ : { *(.*.attributes) *(.ARM.exidx*) *(.eh_frame*) }
}
