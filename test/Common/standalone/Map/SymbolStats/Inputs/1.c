__attribute__((visibility("default")))
int foo() {
  return 5;
}

int bar() {
  return 7;
}

static int baz() {
  return 9;
}

__attribute__((visibility("protected")))
int asdf() {
  return 11;
}

__attribute__((visibility("hidden")))
int foobar = 13;

__attribute__((weak))
int weakfoo() { return 15; }

int main() {
  return foo() + bar() + baz() + asdf();
}