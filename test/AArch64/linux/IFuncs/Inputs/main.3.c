int foo();

int (*foogp)(void) = foo;

int main() {
  int u = foogp();
  return 0;
}