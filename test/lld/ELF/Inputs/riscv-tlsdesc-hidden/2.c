__thread int tls_var = 0;
int foo() { return tls_var; }
