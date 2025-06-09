int bar();

__attribute__((weak))
int fred();

int foo() { return bar() + fred(); }
