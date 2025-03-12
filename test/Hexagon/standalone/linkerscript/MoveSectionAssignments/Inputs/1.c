int bar = 10;
__attribute__((section(".text.foo"))) int foo() { return 0; }
__attribute__((section(".text.bar"))) int boo() { return 0; }
