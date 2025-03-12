// Force a small alignment for each function to make it easier to achieve
// consistent placement across targets. Sections are manually specified as
// -ffunction-sections doesn't seem to play nicely with the align for some
// targets.
__attribute__((aligned(2)))
__attribute__((section(".foo")))
int foo(void) { return 1; }
__attribute__((aligned(2)))
__attribute__((section(".bar")))
int bar(void) { return 2; }
__attribute__((aligned(2)))
__attribute__((section(".baz")))
int baz(void) { return 3; }
