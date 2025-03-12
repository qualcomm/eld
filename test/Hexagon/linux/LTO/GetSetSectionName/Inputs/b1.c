int bar() __attribute__((section(".text.bar"))) {
  return baz();
}
