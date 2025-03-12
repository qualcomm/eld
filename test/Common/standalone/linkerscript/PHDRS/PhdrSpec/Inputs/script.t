PHDRS {
  MYPTPHDR PT_PHDR FILEHDR;
  text PT_LOAD FLAGS(5) FILEHDR PHDRS;
  data PT_LOAD;
}

SECTIONS {
  . = SIZEOF_HEADERS;
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
