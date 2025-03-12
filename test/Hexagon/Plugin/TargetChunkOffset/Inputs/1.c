int bar(char *x) {
  return 0;
}

int foo() {
  bar("hello");
  bar("world");
  return 0;
}
