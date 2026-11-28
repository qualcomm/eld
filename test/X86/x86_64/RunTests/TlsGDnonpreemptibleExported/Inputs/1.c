extern int get_tls(void);

/* Exported by the executable (--export-dynamic) under the same names as the
   shared library's thread-locals. get_tls() uses them only when the library
   is not linked with -Bsymbolic. */
__thread int tls_var = 2;
__thread int tls_var2 = 3;

int main(void) { return get_tls(); }
