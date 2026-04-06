int __real_foo();
int foo();
int bar() { return foo() + __real_foo(); }
