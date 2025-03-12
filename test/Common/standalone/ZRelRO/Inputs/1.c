__attribute__((constructor)) int cfoo() { return 0; }
__attribute__((destructor)) int dbar() { return 0; }
__attribute__((aligned(8192))) int val = 10;
__attribute__((section(".text.foo"))) int foo() { return val; }
__attribute__((section(".text.bar"))) int bar() { return foo(); }
