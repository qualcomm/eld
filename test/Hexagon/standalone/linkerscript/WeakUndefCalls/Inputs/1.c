__attribute__((weak)) int foo();
__attribute__((weak)) int baz();

int bar() { return foo() + baz(); }
