#include <stdio.h>
#define show(x) printf(#x ": %d\n", x);

int foo();
__asm__(".symver bar, foo@V1");
int bar();

int main() {
  show(foo() + bar());
  return 0;
}