__attribute__((weak)) int foo();

int fn() {
  return foo();
}
