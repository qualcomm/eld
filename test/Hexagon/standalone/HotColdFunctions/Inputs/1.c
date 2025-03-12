__attribute__((section(".text.hot"))) int hot1() { return 0; }
__attribute__((section(".text.cold"))) int cold1() { return 0; }
int normal1() { return 0; }
