extern int real_foo(void);
__asm__(".symver real_foo, foo@SOME_VERSION");

int bar(void) { return real_foo(); }
