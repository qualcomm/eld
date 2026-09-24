__asm__(".symver foo_impl, foo@V1");

int foo_impl(void) { return 1; }
