__attribute__((section(".USER_CODE_SECTION")))
int foo(int a) {
  return a;
}
int bar(int b) {
  foo(b);
  return 0;
}
