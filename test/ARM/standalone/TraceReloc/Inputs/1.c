int foo();
int bar() { return foo(); }

int foo() { return bar(); }
