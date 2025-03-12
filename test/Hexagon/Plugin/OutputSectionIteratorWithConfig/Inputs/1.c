__attribute__((weak)) int bar() { return 0; }
int foo() { return bar(); }
int (*ptrbaz)() = &foo;
int (*ptrbar)() = &bar;
