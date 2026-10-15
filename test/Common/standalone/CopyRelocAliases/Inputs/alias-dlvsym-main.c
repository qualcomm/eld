#define _GNU_SOURCE
#include <dlfcn.h>

extern int alias_a;
extern int *address_b(void);
extern int read_b(void);

int main(void) {
  int *volatile copy = &alias_a;
  dlerror();
  int *a = (int *)dlvsym(RTLD_DEFAULT, "alias_a", "V1");
  const char *a_error = dlerror();
  dlerror();
  int *b = (int *)dlvsym(RTLD_DEFAULT, "alias_b", "V1");
  const char *b_error = dlerror();
  if (a_error || b_error || !a || !b)
    return 11;
  if (copy != a || copy != b || copy != address_b())
    return 12;
  if (*copy != 0x13579bdf || *b != 0x13579bdf || read_b() != 0x13579bdf)
    return 13;
  *copy = 0x2468ace0;
  if (*b != 0x2468ace0 || read_b() != 0x2468ace0)
    return 14;
  return 0;
}
