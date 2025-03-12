__attribute__((weak)) int bar();
int baz();
int foo() { return bar() + baz(); }
