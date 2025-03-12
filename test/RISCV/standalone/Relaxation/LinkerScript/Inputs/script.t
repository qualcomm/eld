MEMORY {
  /* text is 0x10 bytes after relaxations */
  MYREGION (rwx) : ORIGIN = 0x100, LENGTH = 0x12
  EMPTYREGION (rwx) : ORIGIN = 0x100, LENGTH = 0x12
}

SECTIONS {
  text : { *(.text) } > MYREGION
}
