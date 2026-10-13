__asm__(".symver wrap_foo_v1, __wrap_foo@V1");

int wrap_foo_v1(void) { return 2; }
