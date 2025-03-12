int foo();
int bar();
extern int a;
extern int b;

int main() { return foo() + bar() + a + b; }