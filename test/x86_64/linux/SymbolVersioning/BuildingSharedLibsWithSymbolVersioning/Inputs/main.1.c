#include <stdio.h>
#define show(x) printf(#x ": %d\n", x);

int foo();
int bar();

int main() {
  show(foo() + bar());
  return 0;
}