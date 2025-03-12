/* Wrapper */
__attribute__((weak)) int __wrap_foo() { return 1; }

/* Real function */
__attribute__((weak)) int foo() { return 1; }
