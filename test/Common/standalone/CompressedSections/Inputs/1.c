int i = 0;
__attribute__((always_inline)) int foo(int j) { return i+j; }
__attribute__((always_inline)) int bar(int j) { return i+j; }
int baz() { return foo(i) + bar(i); }
