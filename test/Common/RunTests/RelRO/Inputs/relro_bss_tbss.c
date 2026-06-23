/* Corner case 4: non-TLS BSS + TBSS.
 * plain_bss is an ordinary BSS variable (SHT_NOBITS, no TLS flag); it must
 * not be confused with .tbss.  tval goes to .tbss.  After the fix the RELRO
 * segment's VirtAddr/Offset must skip both BSS regions and land on the first
 * real RELRO section (.init_array / .got).  Both variables must be
 * accessible after the loader maps the binary. */
#include <stdio.h>

int plain_bss;
__thread int tval;

__attribute__((constructor)) static void init_tval(void) { tval = 42; }

int main(void) {
  printf("bss=%d tbss=%d\n", plain_bss, tval);
  return 0;
}
