__thread int a = 12;
__attribute__((aligned(128))) __thread int b = 13;

int foo() { return a; }
