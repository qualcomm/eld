SECTIONS {
  FOO : { KEEP(EXCLUDE_FILE(*2.o) *(*text*)) }
  .text : { *(*text*) }
}