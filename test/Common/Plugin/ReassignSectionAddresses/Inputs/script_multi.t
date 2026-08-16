PHDRS {
  text PT_LOAD;
  bookkeeping PT_NULL;
}

SECTIONS {
  /DISCARD/ : { *(.ARM.exidx*) }
  .text : { *(.text*) } :text
  .randomized_addrs : { KEEP(*(.randomized_addrs)) } :bookkeeping
  .randomized_addrs2 : { KEEP(*(.randomized_addrs2)) } :bookkeeping
}
