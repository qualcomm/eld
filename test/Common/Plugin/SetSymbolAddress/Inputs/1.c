int foo() { return 1; }
int (*fp)(void) = foo;
int main() { return fp(); }
