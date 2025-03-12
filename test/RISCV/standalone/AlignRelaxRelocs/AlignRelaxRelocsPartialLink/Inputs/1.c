__thread int a = 10;
__attribute__((aligned(32))) int foo() { return a; }
