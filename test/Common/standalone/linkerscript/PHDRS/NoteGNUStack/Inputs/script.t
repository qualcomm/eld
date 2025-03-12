PHDRS {
 TXT PT_LOAD;
 DATA PT_GNU_STACK;
}

SECTIONS {
  .text : { *(.text) } :TXT
  /DISCARD/ : { *(.note*) }
}
