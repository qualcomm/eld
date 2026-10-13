__asm__(".symver foo_v1, foo@V1");
int foo_v1(void) { return 2; }
