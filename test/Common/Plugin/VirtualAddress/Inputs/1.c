int bar();
int foo() { return bar(); }
int bar() { return foo(); }
int baz() { return foo() + bar(); }
