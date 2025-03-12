const int b = 0;
__attribute__((weak)) int foo() { return b; }
__attribute__((weak)) int bar() { return b; }
