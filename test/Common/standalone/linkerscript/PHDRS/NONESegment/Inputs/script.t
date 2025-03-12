PHDRS {
 TXT PT_LOAD;
}

SECTIONS {
  .text : { *(.text*) } :NONE
  /DISCARD/ : { *(.ARM.exidx*) }
}
