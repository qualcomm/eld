#include <stdio.h>

/* Force initial-exec so the linker emits the GOT-indirect (adrp + ldr)
   sequence that --relax rewrites to local-exec (movz + movk) in a
   static executable. The program must behave identically either way. */
__thread int tls_var __attribute__((tls_model("initial-exec"))) = 42;

int main(void) {
  tls_var += 1;
  printf("%d\n", tls_var);
  return 0;
}
