int foo1() { return 0; }
int bar() { return foo(); }
int baz1() { return bar(); }
int func_dso() { return 1; }
