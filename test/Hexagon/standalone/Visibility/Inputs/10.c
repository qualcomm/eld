extern __attribute__((visibility("hidden"))) int __start_bar;
__attribute__((section("foo"))) int bar() { return __start_bar+1; }
