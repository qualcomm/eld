static int b = 10;
int *a = &b;
__attribute__((visibility("hidden"))) int c = 20;
int *d = &c;
