SECTIONS {
  . = . + SIZEOF_HEADERS;
  .text : { *(.text*) }
}
