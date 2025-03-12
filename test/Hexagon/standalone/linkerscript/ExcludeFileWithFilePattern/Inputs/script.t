SECTIONS {
.mydata : {
  *ddrss*.o*(EXCLUDE_FILE( *ddrss_init_sdi*.o* ) .text*)
}
}
