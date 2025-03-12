PHDRS {
  MYPHDR PT_NULL FILEHDR;
  text PT_LOAD;
  data PT_LOAD;
  null1 PT_NULL;
  null2 PT_NULL;
}

SECTIONS {
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
