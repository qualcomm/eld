__asm__(".symver real_foo_impl, __real_foo@V1");

int real_foo_impl(void) { return 3; }
