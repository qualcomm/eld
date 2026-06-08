/* Take the address of the IFUNC `foo` through the GOT (compiler-emitted
 * R_ARM_GOT_PREL when built with -fPIC). */

int foo();

int (*get_foo_from_outside())() { return foo; }
