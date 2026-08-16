PLUGIN_OUTPUT_SECTION_ITER("ReassignSectionAddresses", "ReassignSectionAddresses", "section=.randomized_addrs:hash=sequential:start=100")

PHDRS {
  text PT_LOAD;
  bookkeeping PT_NULL;
}

SECTIONS {
  /DISCARD/ : { *(.ARM.exidx*) }
  .text : { *(.text*) } :text
  .randomized_addrs : { KEEP(*(.randomized_addrs)) } :bookkeeping
}
