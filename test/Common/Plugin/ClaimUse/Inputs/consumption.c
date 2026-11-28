int data = 1;
int *pointer __attribute__((section(".data.claimed"))) = &data;
int unrelated __attribute__((section(".unrelated"))) = 42;
int main(void) { return 0; }
