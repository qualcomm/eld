__attribute__((visibility("hidden"))) int foo_storage = 42;

extern int foo_v1 __attribute__((alias("foo_storage"), visibility("default")));

extern int foo_v2 __attribute__((alias("foo_storage"), visibility("default")));
