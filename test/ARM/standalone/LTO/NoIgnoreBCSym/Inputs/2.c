__attribute__((weak)) extern int foo();
int elf_f() { return foo(); }

int baz() { return 1; }
