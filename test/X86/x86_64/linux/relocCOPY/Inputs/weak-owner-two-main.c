extern void *pa(void), *pb(void);
extern unsigned long va(void), vb(void);

int main(void) {
    if (pa() != pb()) return 1;
    if (va() != 0x0102030405060708UL) return 2;
    if (vb() != 0x0102030405060708UL) return 3;
    return 0;
}
