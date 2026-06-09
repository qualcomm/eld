#include <stdio.h>

/* Multiple TBSS variables with -z separate-loadable-segments: text goes into
   its own R E segment, data into its own RW segment. TBSS must not contaminate
   the text segment flags or prevent the data segment from being writable. */
__thread int tls_a;
__thread int tls_b;
int data = 42;

int main(void) {
  tls_a = 1;
  tls_b = 2;
  data = 99;
  printf("%d %d %d\n", tls_a, tls_b, data);
  return 0;
}
