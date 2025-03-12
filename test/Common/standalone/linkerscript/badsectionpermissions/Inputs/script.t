SECTIONS {
  .text : { *(.text.bar) }
  .data : { *(*.rodata*) *(.data) }
}
