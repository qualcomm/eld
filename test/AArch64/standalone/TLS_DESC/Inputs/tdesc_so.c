/* Preemptible: default visibility, accessible from outside the DSO. */
__thread int tls_preemptible = 10;

/* Non-preemptible: hidden visibility, resolved at DSO link time. */
__attribute__((visibility("hidden"))) __thread int tls_nonpreemptible = 20;

int get_preemptible(void) { return tls_preemptible; }
int get_nonpreemptible(void) { return tls_nonpreemptible; }
