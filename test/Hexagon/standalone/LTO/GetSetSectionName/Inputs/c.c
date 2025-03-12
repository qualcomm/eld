int baz() __attribute__((section(".text.baz"))) {
  return boo();
}
