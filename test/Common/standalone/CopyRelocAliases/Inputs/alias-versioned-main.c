#include "alias-check.h"
extern int vweak;
void lib_store(int);
int *lib_addr_vglobal(void);
int main(void) {
  CHECK(same(lib_addr_vglobal(), &vweak), "library sees another vglobal");
  CHECK(vweak == 0x2145, "initial value was not copied");
  lib_store(7);
  CHECK(vweak == 7, "store through vglobal not seen through vweak");
  return done();
}
