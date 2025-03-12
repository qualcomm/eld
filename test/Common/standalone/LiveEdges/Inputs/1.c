int foo() { return bar(); }

int bar() { return foo(); }

int main() { return foo(); }
