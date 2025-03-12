SECTIONS {
  FOO : { EXCLUDE_FILE(*2.o *lib34.a) *(*text*) }
  BAR : { EXCLUDE_FILE(*lib34.a:) *(*text*) }
  BAZ : { EXCLUDE_FILE(*4.o) *(*text*) }
  OTHERS : { *(*text*) }
}