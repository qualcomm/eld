int foo_impl() { return 11; }

int (*foo_resolver())() { return foo_impl; }

__attribute__((ifunc("foo_resolver"))) int foo();
