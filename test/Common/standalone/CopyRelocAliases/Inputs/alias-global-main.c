#include <stdio.h>

extern int foo_v1;
extern int foo_v2;

int main(void) {
  int *volatile p1 = &foo_v1;
  int *volatile p2 = &foo_v2;

  printf("foo_v1 = %p\n", (void *)p1);
  printf("foo_v2 = %p\n", (void *)p2);

  return p1 != p2;
}
