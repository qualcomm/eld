PHDRS {
  LOAD_RO_NOTE PT_LOAD;
  RO_NOTE PT_NOTE;
  DATA PT_LOAD;
  RW_NOTE PT_NOTE;
}

SECTIONS {
  .note.ro : { *(.note.ro) } :LOAD_RO_NOTE :RO_NOTE
  .note.another.ro : { *(.note.another.ro) } :LOAD_RO_NOTE :RO_NOTE
  .data : { *(.data) } :DATA
  .anotherdata : { *(.anotherdata) } :DATA
  .note.rw : { *(.note.rw) } :DATA :RW_NOTE
}
