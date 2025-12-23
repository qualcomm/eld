int fn();

int foo() { return 11; }

int bar() { return 13; }

int baz() { return fn(); }

int main() {
  return baz();
}
