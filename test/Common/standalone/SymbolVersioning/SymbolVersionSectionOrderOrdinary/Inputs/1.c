extern int provider_fn(void);
int g_data = 42;
int g_bss[16];
int foo() { return provider_fn() + g_data; }
int bar() { return g_bss[0]; }
int baz() { return 5; }
