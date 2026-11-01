int data __attribute__((section(".data"))) = 1;
int *pointer __attribute__((section(".data"))) = &data;
int *another_pointer __attribute__((section(".data"))) = &data;
int main(void) { return 0; }
