int foo();

int main() {
  int (*foop)(void) = foo;
  int u = foop();
  return 0;
}