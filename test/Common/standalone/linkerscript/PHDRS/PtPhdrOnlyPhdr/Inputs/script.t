PHDRS {
  MYPTPHDR PT_PHDR PHDRS;
  text PT_LOAD;
  data PT_LOAD;
}

SECTIONS {
  . = SIZEOF_HEADERS;
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
