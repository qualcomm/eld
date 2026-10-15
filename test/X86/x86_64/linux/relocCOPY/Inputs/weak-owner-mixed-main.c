extern void *small_addr(void), *big_addr(void);
extern unsigned int small_value(void);
extern unsigned long big_value(void);

int main(void) {
    if (small_addr() != big_addr()) return 1;
    if (small_value() != 0x05060708U) return 2;
    if (big_value() != 0x0102030405060708UL) return 3;
    return 0;
}
