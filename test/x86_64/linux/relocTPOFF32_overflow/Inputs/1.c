__thread int u = 1;
__thread int v = 2;

int foo() { return u; }

int main() { return foo() + v; }
