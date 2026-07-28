__attribute__((weak)) __attribute__((visibility("hidden"))) extern int foo;
#ifdef PROTECTED
__attribute__((weak)) __attribute__((visibility("protected"))) extern int bar;
int baz() { return foo + bar; }
#else
int baz() { return foo; }
#endif
