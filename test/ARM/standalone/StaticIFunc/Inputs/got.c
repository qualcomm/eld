int foo();

int (*get_foo_from_outside())() { return foo; }
