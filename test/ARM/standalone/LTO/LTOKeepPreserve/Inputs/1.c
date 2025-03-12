__attribute__((weak)) const int a[] = { 10 };
int blah();
__attribute__((used)) int foo() { return a[0] + blah(); }
