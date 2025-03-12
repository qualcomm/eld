int foo() { return 0; }
int bar() { return foo(); }
__attribute__((section(".text.island_baz"))) int baz() { return foo() + bar(); }
