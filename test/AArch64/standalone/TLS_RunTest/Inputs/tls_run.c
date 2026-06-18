#include <stdio.h>

__thread int tls_global = 42;
static __thread int tls_local = 100;

int main() {
    printf("tls_global = %d\n", tls_global);
    printf("tls_local = %d\n", tls_local);
    tls_global = 99;
    printf("tls_global after write = %d\n", tls_global);
    return 0;
}
