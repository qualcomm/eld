PHDRS {
  seg1 PT_LOAD;
  seg2 PT_LOAD;
}

SECTIONS {
  .text : { *(.text) *(.text.*) } :seg1
  .data : { *(.data) *(.data.*) } :seg2
  .sdata : { *(.sdata) *(.sdata.*) } :seg2
  .bss : { *(.bss) *(.sbss.*) } :seg2
}
