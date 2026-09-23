#include <stdio.h>
#define show(x) printf("%s: %d\n", #x, x);
int foo();

__asm__(".symver foo_main_v1, foo@V1");
int foo_main_v1();

__asm__(".symver foo_main_v2, foo@V2");
int foo_main_v2();

int fn();

int fn_main() { return foo() + foo_main_v1() + foo_main_v2(); }

int main() {
  show(fn());
  show(fn_main());
}
