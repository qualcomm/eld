__attribute__((aligned(4))) int foo() { return bar(); }
__attribute__((aligned(16))) int bar() { return baz(); }
__attribute__((aligned(32))) int baz() { return foo(); }
int main() { return 0; }
