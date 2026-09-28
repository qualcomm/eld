__asm__(".symver impl_weak_def_v1, foo@@V1");
__attribute__((weak)) int impl_weak_def_v1(void) { return 4; }
