extern __thread int tls_var;
int bar() { return tls_var; }
