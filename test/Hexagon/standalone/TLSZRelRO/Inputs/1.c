__attribute__((constructor)) int cfoo() { return 0; }
__attribute__((destructor)) int dbar() { return 0; }
__thread int a = 1;
__thread int b;
int val = 10;
// Access val from GOT.
__attribute__((section(".text.foo"))) int foo() { return val; }
__attribute__((section(".text.bar"))) int bar() { return foo(); }

int useTLS() { b = 1; return a + b; }
