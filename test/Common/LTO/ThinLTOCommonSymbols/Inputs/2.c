int foo[100];

int baz() { return foo[0]; }

int main() { return bar() + baz(); }
