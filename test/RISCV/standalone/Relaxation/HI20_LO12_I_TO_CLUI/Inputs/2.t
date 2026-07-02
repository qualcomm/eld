SECTIONS {
  foo_text 0x7F0 : { *(foo_sec) }
  .data : { *(.data) }
}
