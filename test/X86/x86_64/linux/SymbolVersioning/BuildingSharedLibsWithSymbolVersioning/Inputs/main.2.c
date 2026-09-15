#include <stdio.h>
#define show(x) printf(#x ": %d\n", x);

__asm__(".symver foo, foo@V1");
int foo();

__asm__(".symver bar, bar@V1");
int bar();

int main() {
  show(foo() + bar());
  return 0;
}