PHDRS {
  text PT_LOAD;
  data PT_LOAD;
  null PT_NULL FILEHDR PHDRS;
}

SECTIONS {
  . = SIZEOF_HEADERS;
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
