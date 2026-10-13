__asm__(".symver foo_v1_default, foo@@V1");
int foo_v1_default(void) { return 1; }
