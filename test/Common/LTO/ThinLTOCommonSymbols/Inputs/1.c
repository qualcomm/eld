int baz();
extern int foo;
int bar() { return foo + baz(); }
