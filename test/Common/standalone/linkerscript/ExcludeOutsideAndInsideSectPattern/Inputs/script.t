SECTIONS {
  FOO : { EXCLUDE_FILE(*2.o) *(EXCLUDE_FILE(*3.o) *text*) }
  OTHERS : { *(*text*) }
}