__attribute((weak)) extern int foo();
__attribute((weak)) extern int bar();
int main() { return foo() + bar(); }
