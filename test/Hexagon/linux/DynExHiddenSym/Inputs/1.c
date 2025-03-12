__attribute__((visibility("hidden"))) extern int foo;

int bar() {
  return foo;
}
