extern __attribute__((weak)) int weak_func();
extern int bar();
int foo() {
  return bar() + weak_func();
}
