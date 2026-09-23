int foo() { return 1; }

__asm__(".symver foo2, foo@V2");
int foo2() { return 3; }

__asm__(".symver bar1, bar@V1");
int bar1() { return 5; }

int bar() { return 9; }