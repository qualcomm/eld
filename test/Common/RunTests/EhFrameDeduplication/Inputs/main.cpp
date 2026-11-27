#include <cstdio>

void thrower();

int main() {
  try {
    thrower();
  } catch (int E) {
    std::printf("caught %d\n", E);
  }
  return 0;
}
