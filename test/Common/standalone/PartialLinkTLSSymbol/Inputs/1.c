__thread int a = 1;
__thread int b = 3;

int foo() { return a + b; }
