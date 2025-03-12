static __thread int bar = 10;
static __thread int baz = 20;
int foo() { return bar+baz; }
