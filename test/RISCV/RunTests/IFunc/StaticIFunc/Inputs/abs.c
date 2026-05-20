int foo();

int (*foo_gp)() = foo;

int (*get_foo_from_outside())() { return foo_gp; }
