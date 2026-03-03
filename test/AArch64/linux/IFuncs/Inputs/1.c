int foo1() { return 1; }

int foo2() { return 2; }

static int (*foo_resolver(void))() { return foo1; }

__attribute__((ifunc("foo_resolver"))) int foo();
