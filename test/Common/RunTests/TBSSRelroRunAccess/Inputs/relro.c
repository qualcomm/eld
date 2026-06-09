#include <stdio.h>

/* TBSS (zero-init TLS) followed by writable data.
   With -z relro, if TBSS incorrectly extends the RELRO region into the data
   section, the runtime mprotect will make .data read-only and the write to
   'data' will segfault. */
__thread int tls_zero;
int data = 42;

int main(void) {
  tls_zero = 7;
  data = 99;
  printf("%d %d\n", tls_zero, data);
  return 0;
}
