__attribute__((section(".text.island_baz"))) volatile static int localsymbol();
__attribute__((section(".text.island_baz"))) int baz() { return foo() + bar(); }
__attribute__((section(".text.island_baz"))) int foo() { return 0; }
__attribute__((section(".text.island_baz"))) int bar() { return foo() + localsymbol(); }
__attribute__((section(".text.island_baz"))) volatile static int localsymbol() { return foo(); }
int commonsymbol;
