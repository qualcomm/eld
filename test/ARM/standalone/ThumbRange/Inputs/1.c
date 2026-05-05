int bar();
int baz();
int foo() { return bar() + baz(); }
int bar() { return 0; }
int baz() { return 0; }
