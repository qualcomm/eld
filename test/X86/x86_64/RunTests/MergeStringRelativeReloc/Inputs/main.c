/* main.c -- compiled and linked SECOND.
 *
 * Its .rodata.str1.1 starts with "a"(2) + "c"(2) + "fF"(3) = 7 bytes, followed
 * by the escape strings. Those first three strings are duplicates of pad.c's
 * canonical copies, so they are excluded during string merging. As a result
 * the first escape string "  \n", input offset 7 in main.o, shifts to output
 * offset 0 within main.o's surviving fragment.
 *
 * The g_* pointers live in .data and, in a PIE, are emitted as
 * R_X86_64_RELATIVE dynamic relocations whose addend is the escape string's
 * final address. If the post-merge fixup does not run for x86_64, the addend
 * keeps the pre-merge input offset (7) and points 7 bytes past "  \n", into
 * the following string, so g_nl resolves to the wrong string at runtime. */
#include <stdio.h>
#include <string.h>

const char *g_a = "a";
const char *g_c = "c";
const char *g_fF = "fF";
const char *g_nl = "  \\n";
const char *g_cr = "  \\r";
const char *g_tab = "  \\t";
const char *g_nul = "  \\0";

int main(void) {
  printf("nl: %s\n", strcmp(g_nl, "  \\n") == 0 ? "ok" : "wrong");
  printf("cr: %s\n", strcmp(g_cr, "  \\r") == 0 ? "ok" : "wrong");
  printf("tab: %s\n", strcmp(g_tab, "  \\t") == 0 ? "ok" : "wrong");
  printf("nul: %s\n", strcmp(g_nul, "  \\0") == 0 ? "ok" : "wrong");
  return 0;
}
