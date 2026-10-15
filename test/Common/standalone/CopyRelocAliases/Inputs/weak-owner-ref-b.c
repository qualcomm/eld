extern unsigned long weak_b;
void *pb(void) { return &weak_b; }
unsigned long vb(void) { return weak_b; }
