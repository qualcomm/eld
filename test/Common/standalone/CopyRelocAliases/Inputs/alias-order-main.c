#include "alias-check.h"
int *exe_addr_w(void);
int exe_read_w(void);
int *exe_addr_s(void);
int *lib_addr_s(void);
int *lib_addr_w(void);
int main(void) {
  CHECK(same(exe_addr_w(), exe_addr_s()), "executable sees two objects");
  CHECK(same(lib_addr_s(), exe_addr_s()), "library sees another s_obj");
  CHECK(same(lib_addr_w(), exe_addr_s()), "library sees another w_obj");
  CHECK(exe_read_w() == 0x5345, "initial value was not copied");
  return done();
}
