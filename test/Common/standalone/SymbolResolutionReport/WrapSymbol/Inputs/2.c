int foo() { return 1; }
int asdf() { return 3; }

int __wrap_foo() {
  return asdf();
}
