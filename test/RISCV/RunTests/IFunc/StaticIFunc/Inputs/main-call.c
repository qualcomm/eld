#include <stdio.h>

int foo_impl() { return 11; }

int (*foo_resolver())() { return foo_impl; }

__attribute__((ifunc("foo_resolver"))) int foo();

int call_foo_from_outside();

int main() {
  int (*foo_lp)() = foo;
  int foo_value_from_outside = call_foo_from_outside();
  printf("%d %d %d\n", foo(), foo_lp(), foo_value_from_outside);
  printf("%p %p\n", &foo, foo_lp);
  return 0;
}
