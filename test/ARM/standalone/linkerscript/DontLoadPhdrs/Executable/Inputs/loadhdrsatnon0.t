SECTIONS {
  . = 0x40000000 + SIZEOF_HEADERS;
  .text : { *(.text*) }
}
