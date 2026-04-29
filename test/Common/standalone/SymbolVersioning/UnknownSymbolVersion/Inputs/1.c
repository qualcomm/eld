int real_foo(void) { return 1; }
__asm__(".symver real_foo, foo@DOES_NOT_EXIST");
