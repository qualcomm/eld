extern unsigned int strong_small;
void *small_addr(void) { return &strong_small; }
unsigned int small_value(void) { return strong_small; }
