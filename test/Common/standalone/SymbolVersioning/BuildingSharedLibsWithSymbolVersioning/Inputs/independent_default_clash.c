// A default-versioned symbol `foo@@V2` synthesizes the unversioned alias
// `foo`, which clashes with the independent unversioned `int foo()` body in
// the same object. eld rejects this as a multiple definition rather than
// emitting two default versions (`foo@@V1` and `foo@@V2`) of the same name.
__asm__(".symver foo2, foo@@V2");
int foo2() { return 3; }

int foo() { return 0; }
