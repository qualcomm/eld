PHDRS {
  TXT PT_LOAD;
}

SECTIONS {
.text : { *(.main) } :TXT
}
