__attribute__((aligned(4))) int fn() { return foo() + bar() + baz(); }
__attribute__((aligned(8))) int foo() { return fn() + bar() + baz(); }
__attribute__((aligned(16))) int bar() { return foo() + fn() + baz(); }
__attribute__((aligned(32))) int baz() { return fn() + foo() + bar(); }
