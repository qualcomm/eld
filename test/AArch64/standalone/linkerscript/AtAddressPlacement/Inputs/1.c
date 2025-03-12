int foo() { return 0; }
int bar() { return 0; }
__attribute__((aligned(4))) __attribute__((section("baz@0x28"))) int baz() { return 0; }
