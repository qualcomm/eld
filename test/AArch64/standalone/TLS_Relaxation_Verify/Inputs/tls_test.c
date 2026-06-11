/* Verify TLS relaxations for AArch64.
 * Global-dynamic (TLSDESC) should be relaxed to:
 * - Local Exec (movz+movk) for static executables
 * - Initial Exec (adrp+ldr) for PIE executables
 */
__thread int tls_global = 42;
static __thread int tls_local = 100;

int get_global() { return tls_global; }
int get_local()  { return tls_local; }
