__attribute((section(".bar"))) int foo() {
  return 0;
}

int main() { return foo(); }
