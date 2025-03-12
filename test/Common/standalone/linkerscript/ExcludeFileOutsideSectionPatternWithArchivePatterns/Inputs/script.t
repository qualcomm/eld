SECTIONS {
  FOO_BAZ : { EXCLUDE_FILE(*lib34.a:*4.o *1.o: *2.o) *(*text*) }
  BAR_FRED : { EXCLUDE_FILE(*asdf:*4.o *2.o:) *(*text*) }
  OTHERS : { *(*text*) }
}