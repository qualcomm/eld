int foo() { return 0; }
int bar() { return 0; }
int baz() { return 0; }
int globdata = 20;
int data1 = 10;
int (*ptrfoo)() = &foo;
int *ptrglobdata = &globdata;
int crap() { return data1; }
