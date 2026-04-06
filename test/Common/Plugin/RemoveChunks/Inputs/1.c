int bar();
int foo() { return bar(); }
int bar() { return foo(); }
