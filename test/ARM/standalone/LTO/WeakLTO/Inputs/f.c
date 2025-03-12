__attribute__((weak))  int foo()
{
  return 1234;
}

int bar() {
  return  foo();
}
