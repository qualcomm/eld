PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .mybss : { *(.bss*) *(COMMON) } :A
  .data : { *(.data.*) } :A
  /DISCARD/ : { *(ARM.exidx*) *(.ARM.attributes*) }
}
