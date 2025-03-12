PHDRS {
  MYPHDR1 PT_NULL FILEHDR;
  text PT_LOAD PHDRS;
  data PT_LOAD;
}

SECTIONS {
  . = SIZEOF_HEADERS;
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
