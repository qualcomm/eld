__thread int a = 0;
__thread int b = 0;
int data = 30;
int main() { return a + b + data; }
/* ARM TLS helper stub — satisfies __aeabi_read_tp reference on bare-metal ARM
 */
void *__aeabi_read_tp(void) { return (void *)0; }
