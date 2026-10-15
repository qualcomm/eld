/* Same shape as glibc's environ/__environ: a WEAK alias of a GLOBAL object,
   both exported with a default version. */
int vglobal = 0x2145;
extern int vweak __attribute__((weak, alias("vglobal")));
void lib_store(int v) { vglobal = v; }
int *lib_addr_vglobal(void) { return &vglobal; }
