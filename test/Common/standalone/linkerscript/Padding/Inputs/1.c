__attribute__((aligned(4))) int fn() { return foo() + bar() + baz(); }
__attribute__((aligned(8))) int foo() { return fn() + bar() + baz(); }
__attribute__((aligned(64))) int bar() { return foo() + fn() + baz(); }
__attribute__((aligned(128))) int baz() { return fn() + foo() + bar(); }
