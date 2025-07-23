__attribute__((section(".a"))) __attribute__((aligned(64))) int foo(void) {
  return 1;
}

__attribute__((section(".b"))) __attribute__((aligned(128))) int bar(void) {
  return 2;
}
