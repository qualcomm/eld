int val = 10;
__attribute__((section(".text.foo"))) int foo() { return val; }
__attribute__((section(".text.bar"))) int bar() { return foo(); }
