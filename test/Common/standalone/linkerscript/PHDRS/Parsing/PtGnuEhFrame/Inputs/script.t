PHDRS {
  MYPTPHDR PT_PHDR FILEHDR;
  text PT_LOAD FLAGS(5) FILEHDR PHDRS;
  data PT_LOAD;
  ehframe PT_GNU_EH_FRAME;
}

SECTIONS {
  . = SIZEOF_HEADERS;
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
