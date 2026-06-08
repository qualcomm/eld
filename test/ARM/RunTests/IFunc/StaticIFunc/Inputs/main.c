#include <stdio.h>

int foo_impl() { return 11; }

int (*foo_resolver())() { return foo_impl; }

__attribute__((ifunc("foo_resolver"))) int foo();

int (*get_foo_from_outside())();

int main() {
  int (*foo_lp)() = foo;
  int (*foo_outside)() = get_foo_from_outside();
  printf("%d %d %d\n", foo(), foo_lp(), foo_outside());
  printf("%p %p %p\n", (void *)&foo, (void *)foo_lp, (void *)foo_outside);
  return 0;
}
