int foo();

__asm__(".symver foov1, foo@V1");
int foov1();

__asm__(".symver foov2, foo@V2");
int foov2();

int fn() { return foo() + foov1() + foov2(); }
