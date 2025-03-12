PHDRS {
  TXT PT_LOAD;
  DYNAMIC PT_LOAD;
  DYN PT_DYNAMIC;
}
SECTIONS {
  .text : { *(.text) } :TXT
  .dynamic : { *(.dynamic) } :DYN :DYNAMIC
}
