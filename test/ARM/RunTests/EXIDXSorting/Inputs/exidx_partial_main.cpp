#include <cstdio>

// Declaration for functions defined in exidx_partial_levels.cpp, which is
// compiled separately and combined via partial link (-r).
int level1();

int main() {
  try {
    level1();
  } catch (int v) {
    printf("caught %d\n", v);
  }
  return 0;
}
