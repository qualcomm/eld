PLUGIN_OUTPUT_SECTION_ITER("segmentinfo","SegmentInfo");

PHDRS {
  A PT_LOAD;
  B PT_DYNAMIC;
}
SECTIONS {
  .interp : { *(.interp) } :A
  .dynsym : { *(.dynsym) } :A
  .hash   : { *(.hash) } :A
  .dynstr : { *(.dynstr) } :A
  .text   : { *(.text) } :A
  .dynamic : { _DYNAMIC = .; *(.dynamic) } :A :B
}
