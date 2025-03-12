PHDRS {
 A PT_LOAD;
 B PT_NOTE;
}

SECTIONS {
  .test : { *(.note.test) } :A :B
}
