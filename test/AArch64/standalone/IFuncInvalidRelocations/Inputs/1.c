int foo_impl(int u, int v) { return 1; }

int (*foo_resolver())(int, int) { return foo_impl; }

__attribute__((ifunc("foo_resolver"))) int foo(int, int);
