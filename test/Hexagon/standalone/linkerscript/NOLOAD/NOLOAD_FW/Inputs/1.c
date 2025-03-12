int foo() {
  return 0;
}
__attribute__((aligned(16384))) int mybss[100] = { 0 };

int bar() {
  return 0;
}
