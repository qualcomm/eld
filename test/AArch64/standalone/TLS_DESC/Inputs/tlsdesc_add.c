__thread int tls_var;

int get_tls(void) { return tls_var; }
