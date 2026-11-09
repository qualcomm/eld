extern char **environ;

__attribute__((noinline)) char **pic_env(void) { return environ; }
__attribute__((noinline)) char ***pic_addr(void) { return &environ; }
