extern int baz(char *x);
int foo() {
  baz("hello");
  baz("world");
  return 0;
}