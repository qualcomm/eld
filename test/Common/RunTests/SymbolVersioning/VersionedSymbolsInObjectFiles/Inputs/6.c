int foo();

__asm__(".symver foov1, foo@V1");
int foov1();

int fn() { return foo() + foov1(); }
