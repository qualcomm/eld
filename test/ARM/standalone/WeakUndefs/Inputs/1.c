__attribute__((weak)) extern int foo();
int main() { if (foo) return foo(); }
