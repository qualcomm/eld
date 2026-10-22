extern unsigned long weak_a;
void *pa(void) { return &weak_a; }
unsigned long va(void) { return weak_a; }
