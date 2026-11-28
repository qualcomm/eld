#include "alias-check.h"
extern char small;
long lib_read_big(void);
void *lib_addr_big(void);
int main(void) {
  CHECK(same(&small, lib_addr_big()), "library sees another big");
  CHECK(lib_read_big() == 0x1122334455667788L, "big was not fully copied");
  return done();
}
