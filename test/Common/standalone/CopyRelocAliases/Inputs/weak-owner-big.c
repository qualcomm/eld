extern unsigned long weak_big;
void *big_addr(void) { return &weak_big; }
unsigned long big_value(void) { return weak_big; }
