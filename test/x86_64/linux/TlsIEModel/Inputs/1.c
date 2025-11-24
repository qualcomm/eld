extern __thread int tls_var;
int main(void) { return tls_var; }