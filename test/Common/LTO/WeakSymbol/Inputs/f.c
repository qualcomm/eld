int bar() {
  return  foo();
}
__attribute__((weak))  int foo()
{
  return 1234;
}
