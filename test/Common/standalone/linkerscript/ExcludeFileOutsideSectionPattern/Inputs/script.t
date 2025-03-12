SECTIONS {
  FOO : { EXCLUDE_FILE(*2.o) *(*text*) }
  BAR : { *(*text*) }
}