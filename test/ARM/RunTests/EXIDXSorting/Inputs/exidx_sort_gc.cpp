#include <cstdio>

// dead_func is unreachable from main; with --gc-sections it and its
// .ARM.exidx entry are removed. The remaining EXIDX entries are then
// re-sorted by function address. This test verifies that sorting after
// GC still produces a correct unwind table for the surviving functions.

__attribute__((noinline)) void dead_func() { throw 0; }

__attribute__((noinline)) int level3() {
  throw 99;
  return 0;
}

__attribute__((noinline)) int level2() { return level3(); }

__attribute__((noinline)) int level1() { return level2(); }

int main() {
  try {
    level1();
  } catch (int v) {
    printf("caught %d\n", v);
  }
  return 0;
}
