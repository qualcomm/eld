// Default-versioned symbol (bar@@V1) exercises the object-file versioned
// alias-pair path. See MapDumpGcVersionedAlias.test.
__asm__(".symver bar_impl,bar@@V1");
int foo() { return 0; }
int bar_impl() { return foo(); }
int baz() { return bar_impl(); }
