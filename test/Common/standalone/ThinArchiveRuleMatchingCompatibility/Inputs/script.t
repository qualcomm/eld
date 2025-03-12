SECTIONS {
  foo : { *libdir*:1*.o(*foo*) }
  bar : { *libdir*:1.o(*bar*) }
}
