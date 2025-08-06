__thread int u;
__attribute((aligned(256))) __thread int v;
__thread int a = 12;
__attribute__((aligned(128))) __thread int b = 13;

int foo(void) { return a; }
