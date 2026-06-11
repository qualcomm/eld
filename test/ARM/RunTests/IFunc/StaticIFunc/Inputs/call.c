/* Call the IFUNC `foo` directly: emits R_ARM_CALL. */

extern int foo();

int call_foo_from_outside() { return foo(); }
