int foo();
int baz();

int bar() {
  return 9;
}

int main() {
  return foo() + baz();
}