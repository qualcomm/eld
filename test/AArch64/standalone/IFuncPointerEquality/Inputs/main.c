char *foo_impl() { return "foo"; }

static char *(*foo_resolver())(void) { return foo_impl; }

char *foo() __attribute__((ifunc("foo_resolver")));

char *(*get_foo_from_outside())();

int main() {
  char *(*foo_lp)() = foo;
  char *(*foo_outside)() = get_foo_from_outside();
  return foo == foo_lp && foo_lp == foo_outside;
}
