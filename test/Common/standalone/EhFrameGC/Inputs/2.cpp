extern "C" int baz();

extern "C" int foo() {
  baz();
}
