PHDRS {
  TXT PT_LOAD;
  DYNAMIC PT_LOAD;
  DYN PT_DYNAMIC;
}
SECTIONS {
  .text 0  : { *(.text) } :TXT
  .dynamic : { *(.dynamic) } :DYN :DYNAMIC
}
