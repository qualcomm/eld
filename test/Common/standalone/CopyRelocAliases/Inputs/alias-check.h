#include <stdio.h>
static int failures;
#define CHECK(c, m)                                                            \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL: %s\n", m);                                                 \
      ++failures;                                                              \
    }                                                                          \
  } while (0)
/* Distinct declarations are assumed to have distinct addresses, so compare
   through volatile copies to keep the compiler from folding the check. */
static int same(const void *a, const void *b) {
  const void *volatile x = a, *volatile y = b;
  return x == y;
}
static int done(void) {
  puts(failures ? "FAILED" : "PASSED");
  return failures;
}
