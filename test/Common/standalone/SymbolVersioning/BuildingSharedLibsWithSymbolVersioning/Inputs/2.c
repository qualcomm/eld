__asm__(".symver foo, foo@V1");
int foo() { return 1; }

__asm__(".symver bar, bar@V1");
int bar() { return 3; }

int baz() { return 5;}