int foo();

int (*foogp)(void) = foo;

int main() {
  int (*foop)(void) = foo;
  int u = foop() + foogp();
  return 0;
}