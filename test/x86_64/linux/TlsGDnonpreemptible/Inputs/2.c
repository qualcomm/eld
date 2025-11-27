__thread int tls_var = 42;
__thread int tls_var2 = 50;
int get_tls(void) { return tls_var + tls_var2; }
