extern int foo;
extern int hidden;
static int *dbar = &foo;

int bar() {
  return *dbar;
}
