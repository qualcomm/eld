extern int foo();
__attribute__((weak)) extern int baz;
__attribute__((visibility("hidden"))) int i = 10;
int bar() {
  i = baz;
  return 0;
}
