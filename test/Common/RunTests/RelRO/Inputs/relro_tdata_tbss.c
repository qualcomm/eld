/* Corner case 3: TBSS + TDATA.
 * tval_init goes to .tdata (initialized TLS, FileSiz > 0 in PT_TLS).
 * tval_bss  goes to .tbss  (uninitialized TLS, must be zero at startup).
 * The RELRO segment must not include either TLS section; both values must
 * survive the dynamic loader's RELRO protection pass unchanged. */
#include <stdio.h>

__thread int tval_init = 7;
__thread int tval_bss;

int main(void) {
  printf("tdata=%d tbss=%d\n", tval_init, tval_bss);
  return 0;
}
