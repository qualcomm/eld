extern __attribute__((visibility("hidden"))) int __start_foo;
__attribute__((section("foo"))) int bar() { return __start_foo+1; }
