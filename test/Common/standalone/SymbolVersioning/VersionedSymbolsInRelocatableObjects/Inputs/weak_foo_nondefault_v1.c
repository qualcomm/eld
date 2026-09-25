__asm__(".symver weak_foo_v1, foo@V1");
__attribute__((weak)) int weak_foo_v1(void) { return 6; }
