extern int foo_v1;
int *exe_addr_v1(void) { return &foo_v1; }
int exe_read_v1(void) { return foo_v1; }
