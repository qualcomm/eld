SECTIONS {
  FOO : {
    EXCLUDE_FILE(\ *2.o \
    *3.o) *(.text.foo \ .text.bar \ .text.baz)
  }
  BAR : {
    *(EXCLUDE_FILE(*1.o \
      *3.o) *.text* \)
  }
  BAZ : {
    EXCLUDE_FILE(*1.o \ \
    *2.o)
    *(.text.foo \ .text.bar \
      .text.baz) }
}