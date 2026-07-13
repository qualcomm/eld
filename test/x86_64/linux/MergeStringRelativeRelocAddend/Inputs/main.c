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
 * final address. This is the merge-string RELATIVE case:
 * GNULDBackend::doPostLayout() must propagate the post-merge fragment
 * reference and addend onto each of them.
 *
 * global_var_non_premp is a non-preemptible global read through a GOT entry
 * (clang emits R_X86_64_REX_GOTPCRELX with addend -4, the RIP
 * end-of-instruction bias, for the access in read_it()). The GOT slot itself
 * gets its own R_X86_64_RELATIVE dynamic relocation, whose addend must be the
 * symbol's own address (0 relative to it) -- NOT the -4 from the GOTPCRELX
 * access. This is the GOT RELATIVE case that doPostLayout() must leave
 * untouched: the isMergeStr() guard exists so that copying the merge-string
 * fixup does not also overwrite this GOT slot's addend with -4. */
const char *g_a = "a";
const char *g_c = "c";
const char *g_fF = "fF";
const char *g_nl = "  \\n";
const char *g_cr = "  \\r";
const char *g_tab = "  \\t";
const char *g_nul = "  \\0";

long global_var_non_premp = 42;
long read_it(void) { return global_var_non_premp; }

int main(void) { return 0; }
