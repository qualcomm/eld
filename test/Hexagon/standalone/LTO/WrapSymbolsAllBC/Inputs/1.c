int foo();
int __real_foo();
int main() { return foo() + __real_foo(); }
