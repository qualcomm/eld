PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .mytext : { *(*text*) } :A
  .mydata 0x1024 : { *(.mydata*) } :B
  /DISCARD/ : { *(.ARM.exidx*) }
}
