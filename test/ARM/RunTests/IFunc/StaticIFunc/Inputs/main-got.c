#include <stdio.h>

int foo_impl() { return 11; }

int (*foo_resolver())() { return foo_impl; }

__attribute__((ifunc("foo_resolver"))) int foo();

int (*get_foo_direct())();
int (*get_foo_via_got())();

int main() {
  int (*foo_direct)() = get_foo_direct();
  int (*foo_via_got)() = get_foo_via_got();
  printf("%d %d %d\n", foo(), foo_direct(), foo_via_got());
  /* The direct (PLT) and GOT-loaded pointers must be equal. */
  printf("%p %p %p\n", (void *)foo, (void *)foo_direct, (void *)foo_via_got);
  return 0;
}
