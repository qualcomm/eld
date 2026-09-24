__asm__(".symver foo_v1, foo@V1");
extern int foo(void);
extern int foo_v1(void);

int main(void) { return foo() + foo_v1(); }
