__attribute__((visibility("hidden"))) int storage = 0x13579bdf;
extern int alias_a __attribute__((alias("storage"), visibility("default")));
extern int alias_b __attribute__((alias("storage"), visibility("default")));
int *address_b(void) { return &alias_b; }
int read_b(void) { return alias_b; }
