int __real_foo();
int foo();
int bar() { return foo(); }
int baz() { return __real_foo(); }
