SECTIONS {
  .comment : { *(.comment) }
  /DISCARD/: { *(.note.gnu.property) }
}
