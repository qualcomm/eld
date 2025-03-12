PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .bar : { *(.text.bar) } :B
  /DISCARD/ : { *(.ARM.exidx*) }
  size_of_seg_b = SIZEOF(:B);
  size_of_seg_a = SIZEOF(:A);
}
