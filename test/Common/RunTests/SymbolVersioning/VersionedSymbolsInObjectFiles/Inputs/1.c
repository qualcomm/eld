__asm__(".symver foo1, foo@@V1");
int foo1() { return 1; }
