// Throw chain split into a separate translation unit so that partial linking
// (-r) merges its .ARM.exidx entries with those from main_partial.cpp.
// A correct EXIDX sort after partial link lets the unwinder find the right
// unwind entry for every frame.

__attribute__((noinline)) int level3() {
  throw 99;
  return 0;
}

__attribute__((noinline)) int level2() { return level3(); }

__attribute__((noinline)) int level1() { return level2(); }
