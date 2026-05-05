int bar();
int baz();
__attribute__((constructor)) int foo() { return bar() + baz(); }
