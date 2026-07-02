/* default V2 symbol (used alongside another input's default V1). */
__asm__(".symver impl_v2, bar@@V2");
int impl_v2(void) { return 4; }
