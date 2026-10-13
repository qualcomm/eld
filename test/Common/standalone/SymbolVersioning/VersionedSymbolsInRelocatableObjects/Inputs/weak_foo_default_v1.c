__asm__(".symver weak_foo_v1_default, foo@@V1");
__attribute__((weak)) int weak_foo_v1_default(void) { return 1; }
