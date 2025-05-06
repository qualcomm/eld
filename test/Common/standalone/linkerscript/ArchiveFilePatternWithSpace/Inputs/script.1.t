SECTIONS {
  FOO: { *lib12.a: *1.o(*foo*) }
  BAR: { *lib12.a: *(*bar*) }
}