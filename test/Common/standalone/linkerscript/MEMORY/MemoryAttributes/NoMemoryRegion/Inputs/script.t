PHDRS {
  text PT_LOAD;
}

MEMORY {
  foo (rx) : ORIGIN = 0x10000, LENGTH = 18
}

SECTIONS {
  .text : { *(.text*) } :text
  .data : { *(.*data*) } :text
}
