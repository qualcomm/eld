int foo_impl() { return 1; }

int (*foo_resolver())() { return foo_impl; }

__attribute__((ifunc("foo_resolver"))) int foo();

int (*get_foo_from_outside())();

int main() {
  int (*foo_lp)() = foo;
  int (*foo_outside)() = get_foo_from_outside();
  return foo == foo_lp && foo_lp == foo_outside;
}
