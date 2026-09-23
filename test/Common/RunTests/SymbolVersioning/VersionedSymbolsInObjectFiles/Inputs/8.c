__asm__(".symver foov1, foo@V1");
__attribute__((weak)) int foov1() { return 4; }
