__attribute__((aligned((0x4))))
int foo() { return 0; }

__attribute__((aligned((0x4))))
int bar() { return foo(); }

__attribute__((aligned((0x4))))
int car() { return 0; }
