/* Take the address of the IFUNC `foo` as data: emits R_ARM_ABS32. */

int foo();

int (*foo_gp)() = foo;

int (*get_foo_from_outside())() { return foo_gp; }
