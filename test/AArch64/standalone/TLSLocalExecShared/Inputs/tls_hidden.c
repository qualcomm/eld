__attribute__((visibility("hidden"))) __thread int hidden_tls_var;
int get_tls(void) { return hidden_tls_var; }
