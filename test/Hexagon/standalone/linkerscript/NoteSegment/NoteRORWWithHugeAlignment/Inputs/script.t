SECTIONS {
  .note.ro : { *(.note.ro) }
  .note.another.ro : { *(.note.another.ro) }
  .data : { *(.data) }
  .anotherdata : { *(.anotherdata) }
  .note.rw : { *(.note.rw) }
}
