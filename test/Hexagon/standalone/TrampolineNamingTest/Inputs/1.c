__attribute__((section(".text.foo")))
static int foo1() { return 0; }

__attribute__((section(".text.foo")))
static int foo2() { return foo1(); }

__attribute__((section(".text.bar")))
int bar() { return foo1() + foo2() + foo2(); }