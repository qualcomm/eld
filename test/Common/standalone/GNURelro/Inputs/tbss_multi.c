__thread int tls_a;
__thread int tls_b;
__thread int tls_c;
int data = 42;
int main(void) { return tls_a + tls_b + tls_c + data; }
