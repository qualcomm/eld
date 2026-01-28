#include <stdio.h>
#define show(x) printf(#x ": %d\n", x);

int foo();

int main() {
  show(foo());
  return 0;
}