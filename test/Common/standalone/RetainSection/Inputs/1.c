__attribute__((retain))
int foo() { return 1; }
int bar() { return 3; }

int main() {
  return bar();
}