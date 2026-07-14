#include <cstdio>

// Each function is in its own section so the linker must sort .ARM.exidx
// entries by function address. A correct sort lets the ARM unwinder locate
// the right unwind entry for every frame; an incorrect sort causes the
// exception to go uncaught (terminates instead of printing "caught").

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
