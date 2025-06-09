int bar();

__attribute__((weak))
int fred();

int foo() { return bar() + fred(); }

int bar() { return 3; }

__attribute__((weak))
int fred() { return 7; }
