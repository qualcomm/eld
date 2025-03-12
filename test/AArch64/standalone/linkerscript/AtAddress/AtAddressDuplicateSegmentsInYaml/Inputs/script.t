PHDRS {
  TXT PT_LOAD;
  OTHER PT_LOAD;
}

SECTIONS {
.text : { *(.main*) } :TXT
.other : { *(.other*) } :OTHER
}
