#include "alias-check.h"
extern int foo_v1;
int *lib_addr_v2(void);
int *first_addr_v2(void);
int main(void) {
  CHECK(same(lib_addr_v2(), first_addr_v2()),
        "foo_v2 must keep resolving to the earlier library");
  CHECK(!same(lib_addr_v2(), &foo_v1), "foo_v2 was moved onto foo_v1's copy");
  CHECK(foo_v1 == 0x2145, "initial value was not copied");
  return done();
}
