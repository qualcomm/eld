#include <stdio.h>
#define show(x) printf(#x ": %d\n", x);

__asm__(".symver foo, foo@V2");
int foo();

int bar();

int main() {
  show(foo() + bar());
  return 0;
}