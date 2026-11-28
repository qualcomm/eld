int s_obj = 0x5345;
extern int w_obj __attribute__((weak, alias("s_obj")));
int *lib_addr_s(void) { return &s_obj; }
int *lib_addr_w(void) { return &w_obj; }
