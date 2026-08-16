PHDRS {
  text PT_LOAD;
}

SECTIONS {
  /DISCARD/ : { *(.ARM.exidx*) }
  .text : { *(.text*) } :text
  .randomized_addrs : { KEEP(*(.randomized_addrs)) } :text
}
