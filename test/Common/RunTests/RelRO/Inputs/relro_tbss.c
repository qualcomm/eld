/* TLS BSS variable — contributes a .tbss section (SHT_NOBITS + SHF_TLS).
 * When linked with -z relro this must NOT appear in PT_GNU_RELRO; the
 * segment must start at the first real RELRO section (.init_array / .got).
 * The program just reads and writes the thread-local to prove that TLS
 * itself is still functional after the PT_GNU_RELRO fix. */
#include <stdio.h>

__thread int tval;

__attribute__((constructor)) static void init_tval(void) { tval = 42; }

int main(void) {
  printf("%d\n", tval);
  return 0;
}
