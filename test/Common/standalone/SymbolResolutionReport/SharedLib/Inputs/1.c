int foo() {
  return 1;
}

__attribute__((visibility("hidden")))
int bar() {
  return 3;
}

int baz() {
  return bar();
}