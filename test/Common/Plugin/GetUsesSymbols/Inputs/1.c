int foo() { return 0; }
int bar() { return foo(); }
int baz() { return foo() + bar(); }
