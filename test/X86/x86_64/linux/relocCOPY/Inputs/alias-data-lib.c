/* Two GLOBAL aliases of one hidden, initialized object (issue #2145). */
__attribute__((visibility("hidden"))) int foo_storage = 0x2145;
extern int foo_v1 __attribute__((alias("foo_storage"), visibility("default")));
extern int foo_v2 __attribute__((alias("foo_storage"), visibility("default")));
int *lib_addr_v1(void) { return &foo_v1; }
int *lib_addr_v2(void) { return &foo_v2; }
int lib_read_v2(void) { return foo_v2; }
