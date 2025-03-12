__attribute__((section(".text.hot"))) int hot2() { return 0; }
__attribute__((section(".text.cold"))) int cold2() { return 0; }
int normal2() { return 0; }
