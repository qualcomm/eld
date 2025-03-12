__attribute__((section(".text.hot"))) int foo() { return bar(); }
__attribute__((section(".text.cold"))) int bar() { return foo(); }
__attribute__((section(".text.cold"))) int baz() { return foo(); }

