PHDRS {
  text PT_LOAD FILEHDR PHDRS;
  data PT_LOAD FILEHDR PHDRS;
}

SECTIONS {
  . = SIZEOF_HEADERS;
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
