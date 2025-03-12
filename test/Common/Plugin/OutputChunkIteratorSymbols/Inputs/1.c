int common[100];
char a;
short b;
int c;
long d;
int foo() { return bar() + common[0]; }
int bar() { return foo(); }
int baz() { return foo() + bar(); }
