static __thread int ld_tls = 5;
static __thread int ld_tls2 = 6;
int get_tls(void) { return ld_tls + ld_tls2; }
