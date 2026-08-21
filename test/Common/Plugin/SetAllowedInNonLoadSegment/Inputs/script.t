PHDRS {
  TXT PT_LOAD;
  OVERLAY PT_NULL;
}
SECTIONS {
  . = 0x1000;
  .text : { *(.text*) } :TXT
  . = 0x2000;
  .overlay : { *(.overlay) } :OVERLAY
}
