SECTIONS {
  .text : { *(.text*) }
  . = . + SIZEOF_HEADERS;
}
