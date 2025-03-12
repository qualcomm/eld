char *bar = "foo";
__attribute__((aligned(32))) int foo() { return 0; }
__attribute__((aligned(128))) int car() { return 0; }
