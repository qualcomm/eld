__asm__(".symver foo1, foo@@V1");
__attribute__((weak)) int foo1() { return 1; }
