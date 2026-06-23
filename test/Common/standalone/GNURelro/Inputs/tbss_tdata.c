__thread int tdata1 = 10;
__thread int tdata2 = 20;
__thread int tbss1 = 0;
__thread int tbss2 = 0;
int data = 30;
int main() { return tdata1 + tdata2 + tbss1 + tbss2 + data; }
/* ARM TLS helper stub — satisfies __aeabi_read_tp reference on bare-metal ARM
 */
void *__aeabi_read_tp(void) { return (void *)0; }
