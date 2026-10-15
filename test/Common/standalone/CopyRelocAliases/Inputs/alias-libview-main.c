#include "alias-check.h"
extern int foo_v1;
int *lib_addr_v1(void);
int *lib_addr_v2(void);
int lib_read_v2(void);
int main(void) {
  CHECK(same(lib_addr_v1(), &foo_v1), "library sees another foo_v1");
  CHECK(same(lib_addr_v2(), &foo_v1), "library sees another foo_v2");
  CHECK(foo_v1 == 0x2145, "initial value was not copied");
  foo_v1 = 7;
  CHECK(lib_read_v2() == 7, "store through foo_v1 not seen through foo_v2");
  return done();
}
