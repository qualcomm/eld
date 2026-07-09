// exidx_sort_f_main.cpp
#include <cstdio>
int level1();
int main() {
  try {
    level1();
  } catch (int v) {
    printf("caught %d\n", v);
  }
  return 0;
}
