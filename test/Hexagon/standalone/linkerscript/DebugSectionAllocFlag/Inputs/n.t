
PHDRS {
    NEWLOAD      PT_LOAD;
    NOTE         PT_NOTE;
}

SECTIONS {
 .mycrap : {
  *(.crap*)
 } :NEWLOAD :NOTE

  .debug_str        0 : { *(.debug) }
}
