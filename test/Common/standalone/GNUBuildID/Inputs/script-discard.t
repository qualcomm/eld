SECTIONS {
  .test : { *(.note.test) }
  /DISCARD/ : { *(.note.gnu.build-id) }
}
