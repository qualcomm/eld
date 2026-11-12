extern int data;
int *REFERENCE __attribute__((section(".data"))) = &data;
