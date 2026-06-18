/* strong non-default: bar@V1. */
__asm__(".symver impl, bar@V1");
int impl(void) { return 2; }
