int foo() { return 0; }
int bar() { return foo(); }
int baz() { return bar(); }
int func_dso() { return 1; }
