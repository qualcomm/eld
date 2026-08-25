__attribute__((used)) static char bar_impl;
__attribute__((used)) static char baz_impl;

__asm__(".symver bar_impl, bar@V1");
__asm__(".symver baz_impl, baz@@V1");
