int foo() { return 1; }
int (*fp)(void) = foo;
int data = 5;
int *dp = &data;
int main() { return fp() + *dp; }
