#include <stdio.h>

/* Multiple zero-initialized TLS variables (go into .tbss) followed by
   regular data — exercises the TBSS segment flag and layout fixes. */
__thread int tls_a;
__thread int tls_b;
__thread int tls_c;
int data_x = 10;
int data_y = 20;

int main(void) {
  tls_a = 1;
  tls_b = 2;
  tls_c = tls_a + tls_b;
  printf("%d %d %d %d %d\n", tls_a, tls_b, tls_c, data_x, data_y);
  return 0;
}
