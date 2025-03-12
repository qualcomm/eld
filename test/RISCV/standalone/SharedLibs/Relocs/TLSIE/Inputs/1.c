__thread int bar = 10;
__thread int baz = 20;
__thread int far = 20;
int foo() { return bar+baz+far; }
