__asm__(".symver foo1, foo@@V1");
int foo1() { return 1; }

__asm__(".symver foo2, foo@V2");
int foo2() { return 3; }