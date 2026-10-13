/* Two GLOBAL aliases of one object (issue #2145). The object is bigger than
   Hexagon's small-data threshold, so on every target the executable addresses
   it absolutely and the link needs a COPY relocation. */
__attribute__((visibility("hidden"))) int foo_storage[4] = {1, 2, 3, 4};
extern int foo_v1[4]
    __attribute__((alias("foo_storage"), visibility("default")));
extern int foo_v2[4]
    __attribute__((alias("foo_storage"), visibility("default")));
