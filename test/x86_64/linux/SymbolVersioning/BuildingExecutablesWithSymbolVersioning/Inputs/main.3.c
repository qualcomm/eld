#include <stdio.h>
#define show(x) printf(#x ": %d\n", x);

int foo() {
  return 11;
}

__asm__(".symver foo1, foo@V1");
int foo1();

int main() {
  show(foo() + foo1());
  return 0;
}