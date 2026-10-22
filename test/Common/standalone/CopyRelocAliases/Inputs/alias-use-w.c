extern int w_obj;
int *exe_addr_w(void) { return &w_obj; }
int exe_read_w(void) { return w_obj; }
