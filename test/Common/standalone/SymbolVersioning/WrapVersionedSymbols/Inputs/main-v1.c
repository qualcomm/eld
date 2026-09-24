__asm__(".symver foo_v1, foo@V1");
extern int foo_v1(void);

int main(void) { return foo_v1(); }
