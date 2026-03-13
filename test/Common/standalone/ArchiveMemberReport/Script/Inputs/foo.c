extern int bar(void);

int foo(void) { return bar(); }
int foo_alias(void) __attribute__((alias("foo")));
