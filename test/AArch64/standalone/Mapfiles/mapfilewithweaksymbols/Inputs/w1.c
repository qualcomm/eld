int common;
__attribute__((weak)) __attribute__((section(".bar"))) int foo() {
  return 0;
}

__attribute((section(".bar"))) int bar() {
  return 0;
}
