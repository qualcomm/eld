SECTIONS {
  .note.ro : { *(.note.ro) }
  .note.another.ro : { *(.note.another.ro) }
  .data : { *(.data) }
  .note.rw : { *(.note.rw) }
  .anotherdata : { *(.anotherdata) }
}
