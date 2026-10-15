#include "alias-check.h"
int *exe_addr_v1(void);
int exe_read_v1(void);
int *exe_addr_v2(void);
int *lib_addr_v1(void);
int *lib_addr_v2(void);
int main(void) {
  CHECK(same(exe_addr_v1(), exe_addr_v2()), "executable sees two objects");
  CHECK(same(lib_addr_v1(), exe_addr_v1()), "library sees another foo_v1");
  CHECK(same(lib_addr_v2(), exe_addr_v1()), "library sees another foo_v2");
  CHECK(exe_read_v1() == 0x2145, "initial value was not copied");
  return done();
}
