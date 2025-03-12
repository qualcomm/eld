__attribute__((noinline)) static int bar() { return 0; }
int foo() { return bar() + baz(); }
__attribute__((noinline)) int baz() { return 0; }
