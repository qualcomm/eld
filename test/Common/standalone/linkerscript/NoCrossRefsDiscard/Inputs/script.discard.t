NOCROSSREFS(.text .data)
SECTIONS {
  /DISCARD/ : { *(.data) *(.rodata*) }
}
