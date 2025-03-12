SECTIONS {
  .test : { *(.note.test) }
  .qc.note.build-id : { *(.note.gnu.build-id) }
  /DISCARD/ : { *(.note.gnu.build-id) }
}
