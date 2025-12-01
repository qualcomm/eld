__asm__(".symver foo, foo@V1");
int foo() { return 1; }

__asm__(".symver bar, baz@@V2");
int bar() { return 3; }

int baz() { return 5;}