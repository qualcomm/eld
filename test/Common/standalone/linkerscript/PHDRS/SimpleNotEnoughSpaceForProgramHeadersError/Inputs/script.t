PHDRS {
  text PT_LOAD FILEHDR PHDRS FLAGS(5);
}

SECTIONS {
  .foo : { *(.data.foo) *(.ARM.exidx.text.foo) } :text
}
