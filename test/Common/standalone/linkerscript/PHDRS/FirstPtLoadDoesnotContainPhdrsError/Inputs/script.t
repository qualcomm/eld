PHDRS {
  MYPHDR PT_PHDR FILEHDR PHDRS;
  text PT_LOAD;
  data PT_LOAD FILEHDR PHDRS;
}

SECTIONS {
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
