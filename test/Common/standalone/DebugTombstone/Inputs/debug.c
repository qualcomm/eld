__attribute__((noinline)) void live(void) {}
__attribute__((noinline)) void dead(void) {}
void live2(void) { live(); }
