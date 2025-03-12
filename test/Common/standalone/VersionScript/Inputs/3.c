int foo() { return 0; }
int bar() { return foo(); }
int baz() { return bar(); }
static int local_baz() { return bar(); }
int func_dso() { return bar() + baz() + foo(); }
