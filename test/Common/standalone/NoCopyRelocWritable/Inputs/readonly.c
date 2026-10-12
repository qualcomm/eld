extern int foo;

int *ptr __attribute__((section(".ro_ptr"))) = &foo;
