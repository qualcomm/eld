#include <stdio.h>
#define show(x) printf("%s: %d\n", #x, x);
int foo();

__asm__(".symver foo_main_v1, foo@V1");
int foo_main_v1();

int fn();

int fn_main() { return foo() + foo_main_v1(); }

int main() {
  show(fn());
  show(fn_main());
}
