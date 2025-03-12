int foo() __attribute__((section(".text.foo"))) {
  return bar();
}
