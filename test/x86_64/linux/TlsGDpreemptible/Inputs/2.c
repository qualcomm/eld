extern __thread int tls_var;
int get_tls() { return tls_var; }