/* strong default: bar@@V1 (alias on a separate underlying name so the
 * weak/strong attribute applies cleanly to the alias body). */
__asm__(".symver impl, bar@@V1");
int impl(void) { return 1; }
