/* weak non-default: bar@V1 (weak). */
__asm__(".symver impl, bar@V1");
__attribute__((weak)) int impl(void) { return 2; }
