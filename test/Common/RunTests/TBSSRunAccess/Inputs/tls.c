#include <stdio.h>

__thread int tls_zero = 0;
__thread int tls_also_zero;
int data = 42;

int main(void) {
  tls_zero = 1;
  tls_also_zero = 2;
  printf("%d %d %d\n", tls_zero, tls_also_zero, data);
  return 0;
}
