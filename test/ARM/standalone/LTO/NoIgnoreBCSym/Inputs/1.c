
extern int baz();
int bar() { return baz(); }

__attribute__((weak)) int foo() { return bar(); }
