static int foo_impl() { return 11; }

static int (*foo_resolver())() { return foo_impl; }

__attribute__((ifunc("foo_resolver"))) static int foo();

int (*foo_gp)() = foo;

int main() {
  int (*foo_lp)() = foo;
  return foo == foo_lp && foo_lp == foo_gp;
  return 0;
}
