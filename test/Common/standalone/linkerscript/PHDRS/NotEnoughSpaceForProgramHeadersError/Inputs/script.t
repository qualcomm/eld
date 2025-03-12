PHDRS {
  text PT_LOAD PHDRS;
  data PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) *(.ARM.exidx.text.foo) } :text
  .data : { *(.data.data) } :data
}
