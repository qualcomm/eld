__asm__(".symver foo1, foo@@V1");
int foo1() { return 1; }

__asm__(".symver foo2, foo@V2");
int foo2() { return 3; }

__asm__(".symver bar1, bar@V1");
int bar1() { return 5; }

__asm__(".symver bar2, bar@@V2");
int bar2() { return 9; }